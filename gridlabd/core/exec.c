/** $Id: exec.c 1188 2009-01-02 21:51:07Z dchassin $
 Copyright (C) 2008 Battelle Memorial Institute
 @file exec.c
 @addtogroup exec Main execution loop
 @ingroup core

 The main execution loop sets up the main simulation, initializes the
 objects, and runs the simulation until it either settles to equilibrium
 or runs into a problem.  It also takes care multicore/multiprocessor
 parallelism when possible.  Objects of the same rank will be synchronized
 simultaneously, resources permitting.

 The main processing loop calls each object passing to it a TIMESTAMP
 indicating the desired synchronization time.  The sync() call attempts to
 advance the object's internal clock to the time indicated, and if successful it
 returns the time of the next expected change in the object's state.  An
 object state change is one which requires the equilibrium equations of
 the object to be updated.  When an object's state changes, all the other
 objects in the simulator are given an opportunity to consider the change
 and possibly alter the time of their next state change.  The core
 continues calling objects, advancing the global clock when
 necessary, and continuing in this way until all objects indicate that
 no further state changes are expected.  This is the equilibrium condition
 and the simulation consequently ends.

 @future [Chassin Oct'07]

 There is some value in exploring whether it is necessary to update all
 objects when a particular objects implements a state change.  The idea is
 based on the fact that updates propagate through the model based on known
 relations, such at the parent-child relation or the link-node relation.
 Consequently, it should obvious that unless a value in a related object
 has changed, there can be no significant change to an object that hasn't reached
 it's declared update time.  Thus only the object that "won" the next update
 time and those that are immediately related to it need be updated.  This
 change could result in a very significant improvement in performance,
 particularly in models with many lightly coupled objects.

 @{
 **/

#include <signal.h>
#include <ctype.h>
#include <string.h>
#ifdef WIN32
#include <windows.h>
#include <winbase.h>
#include <direct.h>
#include <sys/timeb.h>
#else
#include <unistd.h>
#endif
#include "output.h"
#include "exec.h"
#include "class.h"
#include "convert.h"
#include "object.h"
#include "index.h"
#include "realtime.h"
#include "module.h"
#include "threadpool.h"
#include "debug.h"
#include "exception.h"
#include "random.h"	
#include "local.h"
#include "schedule.h"
#include "loadshape.h"
#include "enduse.h"
#include "globals.h"
#include "math.h"
#include "time.h"
#include "lock.h"
#include "deltamode.h"
#include "complex.h"

#include "pthread.h"
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
//#include "gldFedAmb.h"

/** The main system initialization sequence
 @return 1 on success, 0 on failure
 **/

int exec_init() {
#if 0
#ifdef WIN32
	char glpathvar[1024];
#endif
#endif

	size_t glpathlen = 0;
	/* setup clocks */
	global_starttime = realtime_now();
	timestamp_set_tz(NULL);

	/* determine current working directory */
	getcwd(global_workdir, 1024);

	/* save locale for simulation */
	locale_push();

#if 0 /* isn't cooperating for strange reasons -mh */
#ifdef WIN32
	glpathlen=strlen("GLPATH=");
	sprintf(glpathvar, "GLPATH=");
	ExpandEnvironmentStrings(getenv("GLPATH"), glpathvar+glpathlen, (DWORD)(1024-glpathlen));
#endif
#endif

	if (global_init() == FAILED)
		return 0;
	return 1;
}

//sjin: GetMachineCycleCount
/*int mc_start_time;
 int mc_end_time;
 int GetMachineCycleCount(void)
 {
 __int64 cycles;
 _asm rdtsc; // won't work on 486 or below - only pentium or above

 _asm lea ebx,cycles;
 _asm mov [ebx],eax;
 _asm mov [ebx+4],edx;
 return cycles;
 }*/
clock_t start, end;

#ifndef _MAX_PATH
#define _MAX_PATH 1024
#endif

#define PASSINIT(p) (p % 2 ? ranks[p]->first_used : ranks[p]->last_used)
#define PASSCMP(i, p) (p % 2 ? i <= ranks[p]->last_used : i >= ranks[p]->first_used)
#define PASSINC(p) (p % 2 ? 1 : -1)

static struct thread_data *thread_data = NULL;
static INDEX **ranks = NULL;
const PASSCONFIG passtype[] = { PC_PRETOPDOWN, PC_BOTTOMUP, PC_POSTTOPDOWN };
static unsigned int pass;
int iteration_counter = 0; /* number of redos completed */

#ifndef NOLOCKS
int64 lock_count = 0, lock_spin = 0;
#endif

extern int stop_now;

//sjin: struct for pthread_create arguments
struct arg_data {
	int thread;
	void *item;
	int incr;
};

INDEX **exec_getranks(void) {
	return ranks;
}

static STATUS setup_ranks(void) {
	OBJECT *obj;
	int i;
	static INDEX *passlist[] = { NULL, NULL, NULL, NULL }; /* extra NULL marks the end of the list */

	/* create index object */
	ranks = passlist;
	ranks[0] = index_create(0, 10);
	ranks[1] = index_create(0, 10);
	ranks[2] = index_create(0, 10);

	/* build the ranks of each pass type */
	for (i = 0; i < sizeof(passtype) / sizeof(passtype[0]); i++) {
		if (ranks[i] == NULL)
			return FAILED;

		/* add every object to index based on rank */
		for (obj = object_get_first(); obj != NULL;
				obj = object_get_next(obj)) {
			/* ignore objects that don't use this passconfig */
			if ((obj->oclass->passconfig & passtype[i]) == 0)
				continue;

			/* add this object to the ranks for this passconfig */
			if (index_insert(ranks[i], obj, obj->rank) == FAILED)
				return FAILED;
			//sjin: print out obj id, pass, rank information
			//else 
			//	printf("obj[%d]: pass = %d, rank = %d\n", obj->id, passtype[i], obj->rank);
		}

		if (global_debug_mode == 0 && global_nolocks == 0)

			/* shuffle the objects in the index */
			index_shuffle(ranks[i]);
	}

	return SUCCESS;
}

char *simtime(void) {
	static char buffer[64];
	char* invalid = "(invalid)";
	return convert_from_timestamp(global_clock, buffer, sizeof(buffer)) > 0 ?
			buffer : invalid;
}

STATUS show_progress(void) {
	output_progress();
	/* reschedule report */
	realtime_schedule_event(realtime_now() + 1, show_progress);
	return SUCCESS;
}

//sjin: implement new ss_do_object_sync for pthreads
static void ss_do_object_sync(int thread, void *item) {
	struct sync_data *data = &thread_data->data[thread];
	OBJECT *obj = (OBJECT *) item;
	TIMESTAMP this_t;

	//printf("thread %d\t%d\t%s\n", thread, obj->rank, obj->name);
	//this_t = object_sync(obj, global_clock, passtype[pass]);

	/* check in and out-of-service dates */
	if (global_clock < obj->in_svc)
		this_t = obj->in_svc; /* yet to go in service */
	else if (global_clock <= obj->out_svc) {
		this_t = object_sync(obj, global_clock, passtype[pass]);
#ifdef _DEBUG
		/* sync dumpfile */
		if (global_sync_dumpfile[0]!='\0')
		{
			static FILE *fp = NULL;
			if (fp==NULL)
			{
				static int tried = 0;
				if (!tried)
				{
					fp = fopen(global_sync_dumpfile,"wt");
					if (fp==NULL)
					output_error("sync_dumpfile '%s' is not writeable", global_sync_dumpfile);
					else
					fprintf(fp,"timestamp,pass,iteration,thread,object,sync\n");
					tried = 1;
				}
			}
			if (fp!=NULL)
			{
				static int64 lasttime = 0;
				static char lastdate[64]="";
				char syncdate[64]="";
				static char *passname;
				static int lastpass = -1;
				char objname[1024];
				if (lastpass!=passtype[pass])
				{
					lastpass=passtype[pass];
					switch(lastpass) {
						case PC_PRETOPDOWN: passname = "PRESYNC"; break;
						case PC_BOTTOMUP: passname = "SYNC"; break;
						case PC_POSTTOPDOWN: passname = "POSTSYNC"; break;
						default: passname = "UNKNOWN"; break;
					}
				}
				if (lasttime!=global_clock)
				{
					lasttime = global_clock;
					convert_from_timestamp(global_clock,lastdate,sizeof(lastdate));
				}
				convert_from_timestamp(this_t<0?-this_t:this_t,syncdate,sizeof(syncdate));
				if (obj->name==NULL) sprintf(objname,"%s:%d", obj->oclass->name, obj->id);
				else strcpy(objname,obj->name);
				fprintf(fp,"%s,%s,%d,%d,%s,%s\n",lastdate,passname,global_iteration_limit-iteration_counter,thread,objname,syncdate);
			}
		}
#endif
	} else
		this_t = TS_NEVER; /* already out of service */

	/* check for "soft" event (events that are ignored when stopping) */
	if (this_t < -1)
		this_t = -this_t;
	else if (this_t != TS_NEVER)
		data->hard_event++; /* this counts the number of hard events */

	/* check for stopped clock */
	if (this_t < global_clock) {
		output_error("%s: object %s stopped its clock (exec)!", simtime(),
				object_name(obj));
		/* TROUBLESHOOT
		 This indicates that one of the objects in the simulator has encountered a
		 state where it cannot calculate the time to the next state.  This usually
		 is caused by a bug in the module that implements that object's class.
		 */
		data->status = FAILED;
	} else {
		/* check for iteration limit approach */
		if (iteration_counter == 2 && this_t == global_clock) {
			output_verbose("%s: object %s iteration limit imminent", simtime(),
					object_name(obj));
		} else if (iteration_counter == 1 && this_t == global_clock) {
			output_error("convergence iteration limit reached for object %s:%d",
					obj->oclass->name, obj->id);
			/* TROUBLESHOOT
			 This indicates that the core's solver was unable to determine
			 a steady state for all objects for any time horizon.  Identify
			 the object that is causing the convergence problem and contact
			 the developer of the module that implements that object's class.
			 */
		}

		/* manage minimum timestep */
		if (global_minimum_timestep
				> 1&& this_t>global_clock && this_t<TS_NEVER)
			this_t = (((this_t - 1) / global_minimum_timestep) + 1)
					* global_minimum_timestep;

		/* if this event precedes next step, next step is now this event */
		if (data->step_to > this_t) {
			//LOCK(data);
			data->step_to = this_t;
			//UNLOCK(data);
		}

		//printf("data->step_to=%d, this_t=%d, name=%s\n", data->step_to, this_t, obj->name);
	}
}

//sjin: implement new ss_do_object_sync_list for pthreads
static void *ss_do_object_sync_list(void *threadarg) {
	LISTITEM *ptr;
	int iPtr;

	struct arg_data *mydata = (struct arg_data *) threadarg;
	int thread = mydata->thread;
	void *item = mydata->item;
	int incr = mydata->incr;

	iPtr = 0;
	for (ptr = (LISTITEM*) item; ptr != NULL; ptr = ptr->next) {
		if (iPtr < incr) {
			ss_do_object_sync(thread, ptr->data);
			iPtr++;
		}
	}
	return NULL;
}

static STATUS init_all(void) {
	OBJECT *obj;
	STATUS rv = SUCCESS;
	output_verbose("exec.c: initializing objects...");

	/* initialize loadshapes */
	if (loadshape_initall() == FAILED || enduse_initall() == FAILED)
		return FAILED;

	TRY
			{
				for (obj = object_get_first(); obj != NULL; obj =
						object_get_next(obj)) {
					if (object_init(obj) == FAILED) {
						THROW("init_all(): object %s initialization failed",
								object_name(obj));
						/* TROUBLESHOOT
						 The initialization of the named object has failed.  Make sure that the object's
						 requirements for initialization are satisfied and try again.
						 */
					}
					if ((obj->oclass->passconfig & PC_FORCE_NAME)
							== PC_FORCE_NAME) {
						if (0 == strcmp(obj->name, "")) {
							output_warning(
									"init: object %s:%d should have a name, but doesn't",
									obj->oclass->name, obj->id);
							/* TROUBLESHOOT
							 The object indicated has been flagged by the module which implements its class as one which must be named
							 to work properly.  Please provide the object with a name and try again.
							 */
						}
					}
				}
			}CATCH (char *msg)
			{
				output_error("init failure: %s", msg);
				/* TROUBLESHOOT
				 The initialization procedure failed.  This is usually preceded
				 by a more detailed message that explains why it failed.  Follow
				 the guidance for that message and try again.
				 */
				rv = FAILED;
			}ENDCATCH;
	return rv;
}

static STATUS commit_all(TIMESTAMP t0) {
	OBJECT *obj;
	STATUS result = SUCCESS;
	TRY
			{
				for (obj = object_get_first(); obj != NULL; obj =
						object_get_next(obj)) {
					if (obj->in_svc <= t0 && obj->out_svc >= t0) {
						if (object_commit(obj) == FAILED) {
							THROW("commit_all(): object %s commit failed",
									object_name(obj));
							/* TROUBLESHOOT
							 The commit function of the named object has failed.  Make sure that the object's
							 requirements for commit'ing are satisfied and try again.  (likely internal state aberations)
							 */
						}
					}
				}
			}CATCH(char *msg)
			{
				output_error("commit() failure: %s", msg);
				/* TROUBLESHOOT
				 The commit'ing procedure failed.  This is usually preceded
				 by a more detailed message that explains why it failed.  Follow
				 the guidance for that message and try again.
				 */
				result = FAILED;
			}ENDCATCH;
	return result;
}

STATUS exec_test(struct sync_data *data, int pass, OBJECT *obj);

STATUS t_setup_ranks(void) {
	return setup_ranks();
}
STATUS t_sync_all(PASSCONFIG pass) {
	struct sync_data sync = { TS_NEVER, 0, SUCCESS };
	TIMESTAMP start_time = global_clock;
	int pass_index = ((int) (pass / 2)); /* 1->0, 2->1, 4->2; NB: if a fourth pass is added this won't work right */

	/* scan the ranks of objects */
	if (ranks[pass_index] != NULL) {
		int i;

		/* process object in order of rank using index */
		for (i = PASSINIT(pass_index); PASSCMP(i, pass_index);
				i += PASSINC(pass_index)) {
			LISTITEM *item;
			/* skip empty lists */
			if (ranks[pass_index]->ordinal[i] == NULL)
				continue;

			for (item = ranks[pass_index]->ordinal[i]->first; item != NULL;
					item = item->next) {
				OBJECT *obj = item->data;
				if (exec_test(&sync, pass, obj) == FAILED)
					return FAILED;
			}
		}
	}

	/* run all non-schedule transforms */
	{
		TIMESTAMP st = scheduletransform_syncall(global_clock,
				XS_DOUBLE | XS_COMPLEX | XS_ENDUSE);	// if (abs(t)<t2) t2=t;
		if (st < sync.step_to)
			sync.step_to = st;
	}

	return SUCCESS;
}

/* this function synchronizes all internal behaviors */
TIMESTAMP syncall_internals(TIMESTAMP t1) {
	TIMESTAMP sc, ls, st, eu, t2;
	sc = schedule_syncall(t1);
	//sc = schedule_syncall_ss(t1);
	ls = loadshape_syncall(t1);	// if (abs(t)<t2) t2=t;
	st = scheduletransform_syncall(t1, XS_SCHEDULE | XS_LOADSHAPE);	// if (abs(t)<t2) t2=t;

	eu = enduse_syncall(t1);	// if (abs(t)<t2) t2=t;
	/* @todo add other internal syncs here */
	t2 = TS_NEVER;
	if (sc < t2)
		t2 = sc;
	if (ls < t2)
		t2 = ls;
	if (st < t2)
		t2 = st;
	if (eu < t2)
		t2 = eu;

	// Round off to the minimum timestep
	if (global_minimum_timestep > 1 && t2 > global_clock && t2 < TS_NEVER)
		t2 = (((t2 - 1) / global_minimum_timestep) + 1)
				* global_minimum_timestep;

	return t2;
}

// global variables - Devendra Shelar
int listen_port = 5000;
int send_port = 5001;
struct hostent *server;
int connFd;

int listenFd, sendFd;
socklen_t len; //store size of the address

struct sockaddr_in svrAdd, clntAdd;
bool loop = false;
double sleepTime = 0.5;

void * listenRTI() {

	output_verbose("Thread No: listening ");
	int count = 0;

	char test[300];
	bzero(test, 301);

	while (!loop) {
		sleep(sleepTime);
		bzero(test, 301);
		output_verbose("reading ");
		read(connFd, test, 300);

		//cout << tester << " count " << ++count << endl;
		output_verbose("listen function %s\n", test);

		if (strcmp(test, "exit") == 0) {
			//mtx.lock();
			loop = true;
			//mtx.unlock();
			break;
		}

	}
	output_verbose("Closing read thread and conn\n");
	close(connFd);
	loop = true;
}

void * sendRTI() {

	output_verbose("Thread No: sending ");
	//send stuff to server
	while (!loop) {
		sleep(sleepTime);
		char s[300] = "cartoon";
		//cin.clear();
		//cin.ignore(256, '\n');
		output_verbose("Enter stuff: ");
		//bzero(s, 301);
		//scanf("%s", s);

		output_verbose("Sending ");
		write(connFd, s, strlen(s));

		if (strcmp(s, "exit") == 0) {
			//mtx.lock();
			loop = true;
			//mtx.unlock();
			break;
		}
	}

	output_verbose("Closing send thread and conn\n");
	close(connFd);
	loop = true;
}

/** This is the main simulation loop
 @return STATUS is SUCCESS if the simulation reached equilibrium,
 and FAILED if a problem was encountered.
 **/
STATUS exec_start(void) {
	//sjin: remove threadpool
	//threadpool_t threadpool = INVALID_THREADPOOL;
	struct sync_data sync = { TS_NEVER, 0, SUCCESS };

	TIMESTAMP start_time = global_clock;
	TIMESTAMP gldSimulationTime = 0;
	TIMESTAMP maxGLDSimulationTime = global_stoptime - start_time;
	//bool hasNS3 = true;
	bool isAttackLaunched = false;
	bool isPVAttackLaunched = false;
	bool isLoadAttackLaunched = false;
	int changeCounter = 0;

	int64 passes = 0, tsteps = 0;
	time_t started_at = realtime_now();
	int j, k;

	//sjin: implement pthreads
	pthread_t *thread_id;
	//sjin: add some variables for pthread implementation
	LISTITEM *ptr;
	int iPtr, incr;
	struct arg_data *arg_data_array;

	/* check for a model */
	if (object_get_count() == 0)

		/* no object -> nothing to do */
		return SUCCESS;

	/* perform object initialization */
	if (init_all() == FAILED) {
		output_error("model initialization failed");
		/* TROUBLESHOOT
		 The initialization procedure failed.  This is usually preceded
		 by a more detailed message that explains why it failed.  Follow
		 the guidance for that message and try again.
		 */
		return FAILED;
	}

	/* establish rank index if necessary */
	if (ranks == NULL && setup_ranks() == FAILED) {
		output_error("ranks setup failed");
		/* TROUBLESHOOT
		 The rank setup procedure failed.  This is usually preceded
		 by a more detailed message that explains why it failed.  Follow
		 the guidance for that message and try again.
		 */
		return FAILED;
	}

	//sjin: print out obj information
	//for (obj=object_get_first(); obj!=NULL; obj=object_get_next(obj))
	//printf("obj id: %d, name = %s, rank = %d\n", obj->id, obj->name, obj->rank);

	/* run checks */
	if (global_runchecks)
		return module_checkall();

	/* compile only check */
	if (global_compileonly)
		return SUCCESS;

	/* enable non-determinism check, if any */
	if (global_randomseed != 0 && global_threadcount > 1)
		global_nondeterminism_warning = 1;

	if (!global_debug_mode) {
		/* schedule progress report event */
		if (global_show_progress) {
			realtime_schedule_event(realtime_now() + 1, show_progress);
		}

		/* set thread count equal to processor count if not passed on command-line */
		if (global_threadcount == 0)
			global_threadcount = processor_count();
		output_verbose("detected %d processor(s)", processor_count());
		output_verbose("using %d helper thread(s)", global_threadcount);

		//sjin: allocate arg_data_array to store pthreads creation argument
		arg_data_array = (struct arg_data *) malloc(
				sizeof(struct arg_data) * global_threadcount);

		/* allocate thread synchronization data */
		thread_data = (struct thread_data *) malloc(
				sizeof(struct thread_data)
						+ sizeof(struct sync_data) * global_threadcount);
		if (!thread_data) {
			output_error("thread memory allocation failed");
			/* TROUBLESHOOT
			 A thread memory allocation failed.
			 Follow the standard process for freeing up memory ang try again.
			 */
			return FAILED;
		}
		thread_data->count = global_threadcount;
		thread_data->data = (struct sync_data *) (thread_data + 1);
		for (j = 0; j < thread_data->count; j++)
			thread_data->data[j].status = SUCCESS;
	} else {
		output_debug("debug mode running single threaded");
		output_message("GridLAB-D entering debug mode");
	}

	/* realtime startup */
	if (global_run_realtime > 0) {
		char buffer[64];
		time(&global_clock);
		output_verbose("realtime mode requires using now (%s) as starttime",
				convert_from_timestamp(global_clock, buffer, sizeof(buffer))
						> 0 ? buffer : "invalid time");
		if (global_stoptime < global_clock)
			global_stoptime = TS_NEVER;
	}

	iteration_counter = global_iteration_limit;
	sync.step_to = global_clock;
	sync.hard_event = 1;

	/* signal handler */
	signal(SIGABRT, exec_sighandler);
	signal(SIGINT, exec_sighandler);
	signal(SIGTERM, exec_sighandler);

	/* initialize delta mode */
	if (!delta_init()) {
		output_error("delta mode initialization failed");
		/* TROUBLESHOOT
		 The initialization of the deltamode subsystem failed.
		 The failure message is preceded by one or more errors that will provide more information.
		 */
	}

	//sjin: GetMachineCycleCount
	//mc_start_time = GetMachineCycleCount();
	start = clock();
	thread_id = (pthread_t *) malloc(global_threadcount * sizeof(pthread_t));


	/* socket communication initialization with NS3 begins */
	char userInput[300];
	pthread_t threadA[2];
	int noThread = 0;

	if (global_ns3_mode) {
		char serverAddress[100] = "127.0.0.1";
		server = gethostbyname(serverAddress);

		if (server == NULL) {
			output_error("Host does not exist");
			return 0;
		}

		//create socket
		listenFd = socket(AF_INET, SOCK_STREAM, 0);

		if (listenFd < 0) {
			output_verbose("Cannot open socket");
			return 0;
		} else {
			output_verbose("Opened socket successfully: listenFd %d", listenFd);
		}

		bzero((char*) &svrAdd, sizeof(svrAdd));

		svrAdd.sin_family = AF_INET;
		svrAdd.sin_addr.s_addr = INADDR_ANY;
		svrAdd.sin_port = htons(listen_port);

		int bindError = 0;
		//bind socket
		if ((bindError = bind(listenFd, (struct sockaddr *) &svrAdd,
				sizeof(svrAdd))) < 0) {
			output_error("Cannot bind, listenFd %d bindError %d", listenFd,
					bindError);
			return 0;
		} else {
			output_verbose("Connection bound, listenFd %d", listenFd);
		}

		listen(listenFd, 5);

		len = sizeof(clntAdd);

		/*//create client skt
		 sendFd = socket(AF_INET, SOCK_STREAM, 0);

		 if(sendFd < 0)
		 {
		 output_error("Cannot open socket");
		 return 0;
		 }

		 bzero((char *) &svrAdd, sizeof(svrAdd));
		 svrAdd.sin_family = AF_INET;

		 bcopy((char *) server -> h_addr, (char *) &svrAdd.sin_addr.s_addr, server -> h_length);

		 svrAdd.sin_port = htons(send_port);

		 connFd = connect(sendFd,(struct sockaddr *) &svrAdd, sizeof(svrAdd));
		 */

		output_message("Waiting for NS3");
		//this is where client connects. svr will hang in this mode until client conn
		connFd = accept(listenFd, (struct sockaddr *) &clntAdd, &len);

		if (connFd < 0) {
			output_error("Cannot connect to NS3!");
			//return 0;
		} else {
			//connFd = sendFd;
			output_verbose("Connection with NS3 successful");
			int64_t maxTime = htonl(maxGLDSimulationTime);
			//write(connFd, &maxTime , sizeof(maxTime));
			write(connFd, &maxGLDSimulationTime, sizeof(maxGLDSimulationTime));
			output_verbose("Maximum GLD Simulation time %d htonl %lld",
					maxGLDSimulationTime, maxTime);
			//pthread_create(&threadA[noThread], NULL, listenRTI, NULL);
			//pthread_create(&threadA[noThread+1], NULL, sendRTI, NULL);
			//noThread+=2;
		}
	}

//	RTIambassador* rtiAmb = rtiAmbassador_new();
//	GldFedAmb* gldFedAmb = gldFedAmb_new();
//	ULong objectHandle = -1;
//	char * federationName = "CYPRESFederation";
//	global_portico_mode = false;
//	if (global_portico_mode) {
//
//		createFederationExecution(rtiAmb, federationName, "../testfom.fed");
//		output_verbose("Created Federation");
//
//		////////////////////////////
//		// 3. join the federation //
//		////////////////////////////
//		// create the federate ambassador and join the federation
//		char * federateName = "GRIDLabDFederate";
//
//		joinFederationExecution(rtiAmb, federateName, federationName,
//				gldFedAmb);
//		output_verbose("Joined Federation as %s", federateName);
//
//		// initialize the handles - have to wait until we are joined
//		initializeHandles(rtiAmb);
//
//		////////////////////////////////
//		// 4. announce the sync point //
//		////////////////////////////////
//		// announce a sync point to get everyone on the same page. if the point
//		// has already been registered, we'll get a callback saying it failed,
//		// but we don't care about that, as long as someone registered it
//		registerFederationSynchronizationPoint(rtiAmb, READY_TO_RUN, "");
//
//		while (isAnnounced(gldFedAmb) == false) {
//			tick(rtiAmb);
//		}
//
//		// WAIT FOR USER TO KICK US OFF
//		// So that there is time to add other federates, we will wait until the
//		// user hits enter before proceeding. That was, you have time to start
//		// other federates.
//		waitForUser();
//
//		///////////////////////////////////////////////////////
//		// 5. achieve the point and wait for synchronization //
//		///////////////////////////////////////////////////////
//		// tell the RTI we are ready to move past the sync point and then wait
//		// until the federation has synchronized on
//		syncAndWait(rtiAmb, gldFedAmb, READY_TO_RUN);
//
//		/////////////////////////////
//		// 6. enable time policies //
//		/////////////////////////////
//		// in this section we enable/disable all time policies
//		// note that this step is optional!
//		//enableTimePolicy();
//		output_verbose("Time Policy Enabled");
//
//		//////////////////////////////
//		// 7. publish and subscribe //
//		//////////////////////////////
//		// in this section we tell the RTI of all the data we are going to
//		// produce, and all the data we want to know about
//		publishAndSubscribe(rtiAmb);
//		output_verbose("Published and Subscribed");
//		output_verbose("Enter something");
//		//scanf("%s", &userInput);
//		output_verbose("User input is : %s", userInput);
//		/////////////////////////////////////
//		// 8. register an object to update //
//		/////////////////////////////////////
//		objectHandle = registerObject(rtiAmb);
//		output_verbose("Registered Object, handle=%lu", objectHandle);
//		output_verbose("Enter something");
//		//scanf("%s", &userInput);
//		output_verbose("User input is : %s", userInput);
//
//	}
	double current_threshold = 10;
	TIMESTAMP oldGLDSimulationTime = -1;
	bool isNewSimulationTime = false;

	/* socket communication initialization with NS3 ends */

	/* main loop exception handler */
	TRY
			{

				/* main loop runs for iteration limit, or when nothing futher occurs (ignoring soft events) */
				int running; /* split into two tests to make it easier to tell what's going on */
				while (running = (sync.step_to <= global_stoptime
						&& sync.step_to < TS_NEVER && sync.hard_event > 0), iteration_counter
						> 0 && (running || global_run_realtime > 0) && !stop_now) {
					/* set time context */
					output_set_time_context(sync.step_to);
					sync.hard_event = (global_stoptime == TS_NEVER ? 0 : 1);
					gldSimulationTime = global_clock - start_time;

					if (oldGLDSimulationTime != gldSimulationTime) {
						isNewSimulationTime = true;
						oldGLDSimulationTime = gldSimulationTime;
					} else
						isNewSimulationTime = false;

					/* realtime support */
					if (global_run_realtime > 0) {
#ifdef WIN32
						struct timeb tv;
						ftime(&tv);
						output_verbose("waiting %d msec", 1000-tv.millitm);
						Sleep(1000-tv.millitm );
						if ( global_run_realtime==1 )
						global_clock = tv.time + global_run_realtime;
						else
						global_clock += global_run_realtime;
#else
						struct timeval tv;
						gettimeofday(&tv);
						output_verbose("waiting %d usec", 1000000 - tv.tv_usec);
						usleep(1000000 - tv.tv_usec);
						if (global_run_realtime == 1)
							global_clock = tv.tv_sec + global_run_realtime;
						else
							global_clock += global_run_realtime;
#endif
						output_verbose("realtime clock advancing to %d",
								(int) global_clock);
					} else
						global_clock = sync.step_to;

					/* operate delta mode if necessary (but only when event mode is active, e.g., not right after init) */
					/* note that delta mode cannot be supported for realtime simulation */
					global_deltaclock = 0;
					if (global_run_realtime == 0) {
						/* determine whether any modules seek delta mode */
						DELTAMODEFLAGS flags = DMF_NONE;
						DT delta_dt = delta_modedesired(&flags);
						TIMESTAMP t = TS_NEVER;
						switch (delta_dt) {
						case DT_INFINITY: /* no dt -> event mode */
							global_simulation_mode = SM_EVENT;
							t = TS_NEVER;
							break;
						case DT_INVALID: /* error dt  */
							global_simulation_mode = SM_ERROR;
							t = TS_INVALID;
							break; /* simulation mode error */
						default: /* valid dt */
							if (global_minimum_timestep > 1) {
								global_simulation_mode = SM_ERROR;
								output_error(
										"minimum_timestep must be 1 second to operate in deltamode");
								t = TS_INVALID;
								break;
							} else {
								if (delta_dt == 0) /* Delta mode now */
								{
									global_simulation_mode = SM_DELTA;
									t = global_clock;
								} else /* Normal sync - get us to delta point */
								{
									global_simulation_mode = SM_EVENT;
									t = global_clock + delta_dt;
								}
							}
							break;
						}
						if (global_simulation_mode == SM_ERROR) {
							output_error(
									"a simulation mode error has occurred");
							break; /* terminate main loop immediately */
						}
						sync.step_to = t;
						if (!((flags & DMF_SOFTEVENT)
								|| (global_simulation_mode != SM_DELTA)))
							sync.hard_event = 1;
					} else
						global_simulation_mode = SM_EVENT;

					/*********************************************** NEW CODE STARTS ***********************************************/
// Devendra Shelar - print objects number
					// sleep for NS3 to allow sending packets
					//sleep(1);
//					output_verbose("Enter something");
//					scanf("%s", &userInput);
//					output_verbose("User input is : %s", userInput);
					output_verbose(
							"exec.c : pass %d, sync %d, gldSimulationTime %d",
							passes, sync.step_to, gldSimulationTime);

					if (global_ns3_mode && isNewSimulationTime) {
						output_verbose("Enter something");
						scanf("%s", &userInput);
						output_verbose("User input is : %s", userInput);

						char receive[300];
						bzero(receive, 301);

						if (passes > 0) {
							output_verbose("receiving ");
							read(connFd, receive, 300);

							output_verbose("listen function %s\n", receive);

							if (strcmp(receive, "exit") == 0) {
								//mtx.lock();
								loop = true;
								//mtx.unlock();
								output_verbose(
										"Closing read thread and conn\n");
								close(connFd);
							} else if (strcmp(receive, "attack") == 0) {
								isAttackLaunched = true;
								current_threshold = 5;
							} else if (strcmp(receive, "pvAttack") == 0) {
								isPVAttackLaunched = true;
							} else if (strcmp(receive, "loadAttack") == 0) {
								isLoadAttackLaunched = true;
							}

						}
					}

//					if (global_portico_mode && isNewSimulationTime) {
//
//						// 9.1 update the attribute values of the instance //
//						updateAttributeValues(rtiAmb, gldFedAmb, objectHandle);
//
//						//reflectAttributeValues(rtiAmb,gldFedAmb, objectHandle);
//
//						// 9.2 send an interaction
//						sendInteraction(rtiAmb, gldFedAmb);
//
//						// 9.3 request a time advance and wait until we get it
//						advanceTime(rtiAmb, gldFedAmb, 1.0);
//						output_verbose(
//								"************************************************************************");
//						printf("RTI Time Advanced to %d\n",
//								getFederateTime(gldFedAmb));
//						printf("Enter something\n");
//						//scanf("%s", &userInput);
//						//output_verbose("User input is : %s", userInput);
//					}

					int di = 1;
					char buffer[1024];
					OBJECT * dobj;
					for (dobj = object_get_first(); dobj != NULL;
							dobj = dobj->next) {
						if (strcmp(dobj->oclass->name, "line_configuration")
								== 0
								|| strcmp(dobj->oclass->name,
										"overhead_line_conductor") == 0
								|| strcmp(dobj->oclass->name, "overhead_line")
										== 0
								|| strcmp(dobj->oclass->name, "switch") == 0
								|| strcmp(dobj->oclass->name, "player") == 0
								|| strcmp(dobj->oclass->name, "load") == 0) {

//							output_verbose(
//									"exec.c : no. of object %d, type %s, gldSimulationTime %d",
//									di++, dobj->oclass->name,
//									gldSimulationTime);
//							output_verbose("exec.c : object name %s ",
//									dobj->name);
							CLASS *oclass = dobj->oclass;
							CLASS *pclass;
							PROPERTY *prop;
							complex *current;		// = & complex(0,0);
							double currentMagnitude = 0;
							//currentMagnitude = current->Mag();

							bool firstTime = true;
							for (pclass = oclass; pclass != NULL; pclass =
									pclass->parent) {
								for (prop = pclass->pmap;
										prop != NULL
												&& prop->oclass
														== pclass->pmap->oclass;
										prop = prop->next) {

									if (prop->unit != NULL
											&& strcmp(prop->unit->name, "V")
													== 0
											&& prop->ptype == PT_complex) {
										complex *pval = object_get_complex(dobj,
												prop);
										if (pval)
											pval->f = A;
									}

//									output_verbose("changeCounter1  %d",
//											changeCounter);
									if (dobj->name != NULL
											&& strcmp(dobj->name, "load4") == 0
											&& strcmp(prop->name,
													"constant_power_A") == 0) {
										output_verbose(
												"****************************************");
										char valuearr[1024];
										char * value = &valuearr;
										int getValue = object_get_value_by_name(
												dobj, prop->name, value, 1024);
										output_verbose(
												"Constant_power_A of pv now %s %d ",
												value, getValue);
										output_verbose(
												"****************************************");
									}

//									output_verbose("changeCounter2 %d", changeCounter);

									if (dobj->name != NULL
											&& (strcmp(dobj->name, "cpPlayerPV")
													== 0
													|| strcmp(dobj->name,
															"load4") == 0)) {

										if (isPVAttackLaunched
												&& gldSimulationTime > 4
												&& changeCounter < 2) {

//											output_verbose(
//													"Looking for cpPlayerPV obj %s prop %s cc %d time %d attack? %d",
//													dobj->name, prop->name,
//													changeCounter,
//													gldSimulationTime,
//													isPVAttackLaunched);

//											output_verbose(
//													"Tried to do something about the pv attack");
											if (strcmp(prop->name, "file")
													== 0) {
//												file "models/pvAttack.csv"
												output_verbose(
														"****************************************");

												output_verbose(
														"Changed value of player file to pvAttack.csv");
												object_set_value_by_name(dobj,
														prop->name,
														"pvAttack.csv");
												output_verbose(
														"****************************************");
												//changeCounter++;
											} else if (strcmp(prop->name,
													"constant_power_A") == 0) {
												output_verbose(
														"****************************************");

												complex *newPower =
														object_get_complex_by_name(
																dobj,
																prop->name);
												newPower->r = 400000;
												newPower->i = 400000;
//												newPower.f = CNOTATION_DEFAULT;
//												object_set_complex_by_name(dobj,
//														prop->name, newPower);
												output_verbose(
														"Changed value of pv power to %d + %d j",
														newPower->r,
														newPower->i);
												complex *setValue =
														object_get_complex_by_name(
																dobj,
																prop->name);
												output_verbose(
														"New value of pv power to %d + %d j",
														setValue->r,
														setValue->i);

												output_verbose(
														"****************************************");
												//changeCounter++;
											}
										} else if (isLoadAttackLaunched
												&& gldSimulationTime > 4
												&& changeCounter < 2) {
											if (strcmp(prop->name,
													"constant_power_A") == 0) {
												output_verbose(
														"****************************************");

												complex *newPower =
														object_get_complex_by_name(
																dobj,
																prop->name);
												newPower->r *= 2;
												newPower->i *= 2;
//
												output_verbose(
														"Changed value of load power to %d + %d j",
														newPower->r,
														newPower->i);
												complex *setValue =
														object_get_complex_by_name(
																dobj,
																prop->name);
												output_verbose(
														"New value of load power to %d + %d j",
														setValue->r,
														setValue->i);

												output_verbose(
														"****************************************");
//												changeCounter++;
											}
										}

										if (strcmp(dobj->name,
												"circuitBreakerLine") == 0
												&& firstTime) {
											current =
													object_get_complex_by_name(
															dobj,
															"current_in_A");
											double cr = current->r;
											double ci = current->i;
											//double r = current->Re();
											//double i = current->Im();
											//if(current != 0 )
											//	currentMagnitude = current->Mag();
											currentMagnitude = sqrt(
													cr * cr + ci * ci);
											output_verbose(
													"Complex values real %f imag %f magnitude %f threshold %f",
													cr, ci, currentMagnitude,
													current_threshold);
											firstTime = false;
										}

										if (object_get_value_by_name(dobj,
												prop->name, buffer,
												sizeof(buffer)) > 0
												&& strcmp(buffer, "") != 0) {
//											output_verbose(
//													"\t\t\t\t<%s>%s</%s>",
//													prop->name, buffer,
//													prop->name);

											//output_verbose("************************************************ chutiyapa %d\n" );
											//gldFedAmb_print();
											//printObjectInstance(gldFedAmb);

											char propValue[] = "1000000000";
											char zeroValue[] = "0";
											if (strcmp(prop->name,
													"current_in_A") == 0) {
//												output_verbose(
//														"\t\t\t\tYaaaaahoooo<%s>%s</%s>",
//														prop->name, buffer,
//														prop->name);

											}
											if (strcmp(prop->name, "resistance")
													== 0) {

												//if (currentMagnitude > current_threshold){
												if (isAttackLaunched
														&& gldSimulationTime
																>= 0
														&& changeCounter < 0) {
													object_set_value_by_name(
															dobj, prop->name,
															propValue);
													changeCounter++;
												}

											} else if (strcmp(prop->name,
													"status") == 0
													|| strcmp(prop->name,
															"phase_A_state")
															== 0
													|| strcmp(prop->name,
															"phase_B_state")
															== 0
													|| strcmp(prop->name,
															"phase_C_state")
															== 0) {

												//if (currentMagnitude > current_threshold){
												if (isAttackLaunched
														&& gldSimulationTime > 4
														&& changeCounter < 2) {
													if (isAttackLaunched || isLoadAttackLaunched
															&& (currentMagnitude
																	> current_threshold)) {
														char statusValue[] =
																"OPEN";
														output_verbose(
																"Changed value of switch to open");
														object_set_value_by_name(
																dobj,
																prop->name,
																statusValue);
														changeCounter++;
													}
												}

											} else if (strcmp(prop->name,
													"emergency_rating") == 0) {
												object_set_value_by_name(dobj,
														prop->name, zeroValue);
											} else if (strcmp(prop->name,
													"file") == 0) {
												if (isAttackLaunched
														&& changeCounter <= 2
														&& gldSimulationTime
																> 1) {
													if (strcmp(dobj->name,
															"cpPlayer") == 0) {
														//object_set_value_by_name(dobj,prop->name, "models/c_p_A_4_uc5_fraud.csv");
													}
													if (strcmp(dobj->name,
															"cpPlayerPV")
															== 0) {
														//object_set_value_by_name(dobj,prop->name, "models/c_p_A_4_pv_fraud.csv");
													}
													//changeCounter++;
												}
											}
										}
									}
								}
							}
							//break;

						} else {
							di++;
							//output_verbose("exec.c : Could not find load -- object no. of object %d", di++);
						}

					}

					/*********************************************** NEW CODE ENDS ***********************************************/

					/* synchronize all internal schedules */
					{
						TIMESTAMP t = syncall_internals(global_clock);
						if (t != TS_NEVER && t <= global_clock) {
							THROW("internal property sync failure");
							/* TROUBLESHOOT
							 An internal property such as schedule, enduse or loadshape has failed to synchronize and the simulation aborted.
							 This message should be preceded by a more informative message that explains which element failed and why.
							 Follow the troubleshooting recommendations for that message and try again.
							 */
						} else if (t < sync.step_to)
							sync.step_to = t;
					}

					if (!global_debug_mode) {
						for (j = 0; j < thread_data->count; j++) {
							thread_data->data[j].hard_event = 0;
							thread_data->data[j].step_to = TS_NEVER;
						}
					}

					/* scan the ranks of objects */
					for (pass = 0; ranks[pass] != NULL; pass++) {
						int i;

						/* process object in order of rank using index */
						for (i = PASSINIT(pass); PASSCMP(i, pass);
								i += PASSINC(pass)) {
							/* skip empty lists */
							if (ranks[pass]->ordinal[i] == NULL)
								continue;

							if (global_debug_mode) {
								LISTITEM *item;
								for (item = ranks[pass]->ordinal[i]->first;
										item != NULL; item = item->next) {
									OBJECT *obj = item->data;
									if (exec_debug(&sync, pass, i, obj)
											== FAILED)
										THROW("debugger quit");
								}
							} else {
								//sjin: remove threadpool
								//tp_exec(threadpool, ranks[pass]->ordinal[i]);

								//sjin: if global_threadcount == 1, no pthread multhreading
								if (global_threadcount == 1) {
									for (ptr = ranks[pass]->ordinal[i]->first;
											ptr != NULL; ptr = ptr->next) {
										OBJECT *obj = ptr->data;
										//output_verbose("pass %d objectName %s", pass, obj->name);
										ss_do_object_sync(0, ptr->data);
										//output_verbose("I am trying to sync here baby");

										if (obj->valid_to == TS_INVALID) {
											//Get us out of the loop so others don't exec on bad status
											break;
										}
										//printf("%d %s %d\n", obj->id, obj->name, obj->rank);
									}
									//printf("\n");
								} else { //sjin: implement pthreads
									incr =
											(int) ceil(
													(float) ranks[pass]->ordinal[i]->size
															/ global_threadcount);
									//printf("pass %d, incr = %d, size = %d\n", pass, incr, ranks[pass]->ordinal[i]->size);
									//thread_id = (pthread_t *) malloc(global_threadcount * sizeof(pthread_t));
									// copying this further up
									/* if the number of objects is less than or equal to the number of threads, each thread process one object */
									if (incr <= 1) {
										//printf("incr = %d\n", incr);
										iPtr = -1;
										for (ptr =
												ranks[pass]->ordinal[i]->first;
												ptr != NULL; ptr = ptr->next) {
											iPtr++;
											//printf("<= %d: ranks[%d]->ordinal[%d]->size = %d\n", iPtr, pass, i, ranks[pass]->ordinal[i]->size);
											arg_data_array[iPtr].thread = iPtr;
											arg_data_array[iPtr].item = ptr;
											arg_data_array[iPtr].incr = 1;
											//printf("<=1: size = %d, iPtr = %d, incr = %d, thread id = %d\n", ranks[pass]->ordinal[i]->size, iPtr, incr, &thread_id[iPtr]);
											pthread_create(&thread_id[iPtr],
											NULL, ss_do_object_sync_list,
													(void *) &arg_data_array[iPtr]);
											//pthread_join(thread_id[iPtr], NULL);
											//pthread_cancel(thread_id[iPtr]);
											//printf("<=1:\tthread_id[%d].p = %p\t.x = %lu\n", iPtr, (void *) thread_id[iPtr].p, (unsigned long) thread_id[iPtr].x);
										}
										for (j = 0; j < iPtr + 1; j++)
											pthread_join(thread_id[j],
											NULL);
										/* if the number of objects is greater than the number of threads, each thread process the same number of
										 objects (incr), except that the last thread may process less objects */
									} else {
										//printf("incr = %d\n", incr);
										iPtr = -1;
										j = 0;
										for (ptr =
												ranks[pass]->ordinal[i]->first;
												ptr != NULL; ptr = ptr->next) {
											iPtr++;
											if (iPtr % incr == 0) {
												//printf("> thread %d, start from object %d: ranks[%d]->ordinal[%d]->size = %d\n", j, iPtr, pass, i, ranks[pass]->ordinal[i]->size);
												arg_data_array[j].thread = j;
												arg_data_array[j].item = ptr;
												arg_data_array[j].incr = incr;
												//printf(">1: size = %d, iPtr = %d, incr = %d, thread id = %d\n", ranks[pass]->ordinal[i]->size, iPtr, incr, thread_id[j]);
												pthread_create(&thread_id[j],
												NULL, ss_do_object_sync_list,
														(void *) &arg_data_array[j]);
												//pthread_join(thread_id[j], NULL);
												//pthread_cancel(thread_id[j]);
												//printf("> 1:\tthread_id[%d].p = %p\t.x = %lu\n", j, (void *) thread_id[j].p, (unsigned long) thread_id[j].x);
												j++;
											}
										}
										for (k = 0; k < j; k++) {
											//printf("k=%d, j=%d\n",k,j);
											pthread_join(thread_id[k],
											NULL);
											//printf("done!\n");
										}
									}

									//if (iPtr+1 < global_threadcount)
									/*if (incr <= 1)
									 for (j = 0; j < iPtr+1; j++)
									 pthread_join(thread_id[j], NULL);
									 else
									 for (j = 0; j < global_threadcount; j++)
									 pthread_join(thread_id[j], NULL);*/
								} //sjin: end of implementing pthreads

								for (j = 0; j < thread_data->count; j++) {
									if (thread_data->data[j].status == FAILED) {
										sync.status = FAILED;
										// close the socket
										if (global_ns3_mode) {
											char send[300] =
													"convergenceFailure";
											write(connFd, send, strlen(send));
											output_verbose("Sending %s ", send);
											close(connFd);
										}
										THROW("synchronization failed");
									}
								}
							}
						}

						/* run all non-schedule transforms */
//						output_verbose(
//								"exec.c : calling scheduletransform_syncall");
						{
							/*TIMESTAMP st = scheduletransform_syncall(global_clock,XS_DOUBLE|XS_COMPLEX|XS_ENDUSE);// if (abs(t)<t2) t2=t;
							 if (st<sync.step_to)
							 sync.step_to = st;*/
						}
					}

					if (!global_debug_mode) {
						for (j = 0; j < thread_data->count; j++) {
							sync.hard_event += thread_data->data[j].hard_event;
							if (thread_data->data[j].step_to < sync.step_to)
								sync.step_to = thread_data->data[j].step_to;
						}

						/* report progress */
//						output_verbose(
//								"exec.c : calling realtime_run_schedule");
						realtime_run_schedule();
					}

					/* count number of passes */
					passes++;

					/* check for clock advance */
					if (sync.step_to != global_clock) {
						if (commit_all(global_clock) == FAILED) {
							output_error("model commit failed");
							/* TROUBLESHOOT
							 The commit procedure failed.  This is usually preceded
							 by a more detailed message that explains why it failed.  Follow
							 the guidance for that message and try again.
							 */
							return FAILED;
						}

						/* reset iteration count */
						iteration_counter = global_iteration_limit;

						/* count number of timesteps */
						tsteps++;
					}

					/* check iteration limit */
					else if (--iteration_counter == 0) {
						output_error(
								"convergence iteration limit reached at %s (exec)",
								simtime());
						/* TROUBLESHOOT
						 This indicates that the core's solver was unable to determine
						 a steady state for all objects for any time horizon.  Identify
						 the object that is causing the convergence problem and contact
						 the developer of the module that implements that object's class.
						 */
						sync.status = FAILED;
						THROW("convergence failure");
					}

					/* in server mode dump the mode after the first timestep */
					if (global_run_realtime > 0 && tsteps == 1
							&& global_dumpfile[0] != '\0') {
						if (!saveall(global_dumpfile))
							output_error("dump to '%s' failed",
									global_dumpfile);
						/* TROUBLESHOOT
						 An attempt to create a dump file failed.  This message should be
						 preceded by a more detailed message explaining why if failed.
						 Follow the guidance for that message and try again.
						 */
						else
							output_message(
									"initial model dump to '%s' complete",
									global_dumpfile);
					}

					/* handle delta mode operation */
					if (global_simulation_mode == SM_DELTA
							&& sync.step_to >= global_clock) {
						DT deltatime = delta_update();
						if (deltatime == DT_INVALID) {
							output_error(
									"delta_update() failed, deltamode operation cannot continue");
							/*  TROUBLESHOOT
							 An error was encountered while trying to perform a deltamode update.  Look for
							 other relevant deltamode messages for indications as to why this may have occurred.
							 If the error persists, please submit your code and a bug report via the trac website.
							 */
							global_simulation_mode = SM_ERROR;
							break;
						}
						sync.step_to = global_clock + deltatime;
						global_simulation_mode = SM_EVENT;
					}
					output_verbose("exec.c : sync.step_to %d global_clock %d",
							sync.step_to, global_clock);

					if (global_ns3_mode && isNewSimulationTime) {
						output_verbose("Enter stuff to send: ");
						char send[300] = "sent to NS3";
						//scanf("%s", send);

						output_verbose("Sending %s ", send);
						write(connFd, send, strlen(send));
					}

				}

				/* disable signal handler */
				signal(SIGINT, NULL);

				/* check end state */
				if (sync.step_to == TS_NEVER) {
					char buffer[64];
					output_verbose("simulation at steady state at %s",
							convert_from_timestamp(global_clock, buffer,
									sizeof(buffer)) ? buffer : "invalid time");
				}

			}CATCH(char *msg)
			{
				output_error("exec halted: %s", msg);
				sync.status = FAILED;
				/* TROUBLESHOOT
				 This indicates that the core's solver shut down.  This message
				 is usually preceded by more detailed messages.  Follow the guidance
				 for those messages and try again.
				 */
			}ENDCATCH

// close the socket
	if (global_ns3_mode) {
		char send[300] = "exit";
		write(connFd, send, strlen(send));
		output_verbose("Sending %s ", send);
		close(connFd);
	}

//	if (global_portico_mode) {
//		cleanUp(rtiAmb, gldFedAmb, federationName, objectHandle);
//		output_verbose("Cleaned RTI up");
//	}

//sjin: GetMachineCycleCount
//mc_end_time = GetMachineCycleCount();
//printf("%ld\n",(mc_end_time-mc_start_time));
	end = clock();
//printf("%f\n", (double)(end - start) / (double)CLOCKS_PER_SEC);

	/* deallocate threadpool */
	if (!global_debug_mode) {
		//sjin: remove threadpool
		//tp_release(threadpool);
		free(thread_data);
		thread_data = NULL;

#ifdef NEVER
		/* wipe out progress report */
		if (!global_keep_progress)
		output_raw("                                                           \r");
#endif
	}

	/* report performance */
	if (global_profiler && sync.status == SUCCESS) {
		loop = true;
		char exitChar[] = "exit";
		write(connFd, exitChar, strlen(exitChar));

		double elapsed_sim = (timestamp_to_hours(
				global_clock < start_time ? start_time : global_clock)
				- timestamp_to_hours(start_time));
		double elapsed_wall = (double) (realtime_now() - started_at + 1);
		double sync_time = 0;
		double sim_speed = object_get_count() / 1000.0 * elapsed_sim
				/ elapsed_wall;
		CLASS *cl;
		DELTAPROFILE *dp = delta_getprofile();
		double delta_runtime = 0, delta_simtime = 0;
		if (global_threadcount == 0)
			global_threadcount = 1;
		for (cl = class_get_first_class(); cl != NULL; cl = cl->next)
			sync_time += ((double) cl->profiler.clocks) / CLOCKS_PER_SEC;
		sync_time /= global_threadcount;
		delta_runtime =
				dp->t_count > 0 ?
						(dp->t_preupdate + dp->t_update + dp->t_postupdate)
								/ CLOCKS_PER_SEC :
						0;
		delta_simtime = dp->t_count * (double) dp->t_delta
				/ (double) dp->t_count / 1e9;

		output_profile("\nCore profiler results");
		output_profile("=====================\n");
		output_profile("Total objects           %8d objects",
				object_get_count());
		output_profile("Parallelism             %8d thread%s",
				global_threadcount, global_threadcount > 1 ? "s" : "");
		output_profile("Total time              %8.1f seconds", elapsed_wall);
		output_profile("  Core time             %8.1f seconds (%.1f%%)",
				(elapsed_wall - sync_time - delta_runtime),
				(elapsed_wall - sync_time - delta_runtime) / elapsed_wall
						* 100);
		if (dp->t_count > 0)
			output_profile(
					"  Deltamode time        %8.1f seconds/thread (%.1f%%)",
					delta_runtime, delta_runtime / elapsed_wall * 100);
		output_profile("  Model time            %8.1f seconds/thread (%.1f%%)",
				sync_time, sync_time / elapsed_wall * 100);
		output_profile("Simulation time         %8.0f days", elapsed_sim / 24);
		if (sim_speed > 10.0)
			output_profile(
					"Simulation speed         %7.0lfk object.hours/second",
					sim_speed);
		else if (sim_speed > 1.0)
			output_profile(
					"Simulation speed         %7.1lfk object.hours/second",
					sim_speed);
		else
			output_profile(
					"Simulation speed         %7.0lf object.hours/second",
					sim_speed * 1000);
		output_profile("Syncs completed         %8d passes", passes);
		output_profile("Time steps completed    %8d timesteps", tsteps);
		output_profile("Convergence efficiency  %8.02lf passes/timestep",
				(double) passes / tsteps);
#ifndef NOLOCKS
		output_profile("Memory lock contention   %7.01lf%%",
				(lock_spin > 0 ?
						(1 - (double) lock_count / (double) lock_spin) * 100 : 0));
#endif
		output_profile("Average timestep         %7.0lf seconds/timestep",
				(double) (
						global_clock < start_time ?
								0 : global_clock - start_time) / tsteps);
		output_profile("Simulation rate          %7.0lf x realtime",
				(double) (
						global_clock < start_time ?
								0 : global_clock - start_time) / elapsed_wall);
		if (dp->t_count > 0) {
			double total = dp->t_preupdate + dp->t_update + dp->t_interupdate
					+ dp->t_postupdate;
			output_profile("\nDelta mode profiler results");
			output_profile("===========================\n");
			output_profile("Active modules          %s", dp->module_list);
			output_profile("Initialization time     %8.1lf seconds",
					(double) (dp->t_init) / (double) CLOCKS_PER_SEC);
			output_profile("Number of updates       %8"FMT_INT64"u",
					dp->t_count);
			output_profile("Average update timestep %8.4lf ms",
					(double) dp->t_delta / (double) dp->t_count / 1e6);
			output_profile("Minumum update timestep %8.4lf ms",
					dp->t_min / 1e6);
			output_profile("Maximum update timestep %8.4lf ms",
					dp->t_max / 1e6);
			output_profile("Total deltamode simtime %8.1lf s",
					delta_simtime / 1000);
			output_profile("Preupdate time          %8.1lf s (%.1f%%)",
					(double) (dp->t_preupdate) / (double) CLOCKS_PER_SEC,
					(double) (dp->t_preupdate) / total * 100);
			output_profile("Object update time      %8.1lf s (%.1f%%)",
					(double) (dp->t_update) / (double) CLOCKS_PER_SEC,
					(double) (dp->t_update) / total * 100);
			output_profile("Interupdate time        %8.1lf s (%.1f%%)",
					(double) (dp->t_interupdate) / (double) CLOCKS_PER_SEC,
					(double) (dp->t_interupdate) / total * 100);
			output_profile("Postupdate time         %8.1lf s (%.1f%%)",
					(double) (dp->t_postupdate) / (double) CLOCKS_PER_SEC,
					(double) (dp->t_postupdate) / total * 100);
			output_profile("Total deltamode runtime %8.1lf s (100%%)",
					delta_runtime);
			output_profile("Simulation rate         %8.1lf x realtime",
					delta_simtime / delta_runtime / 1000);
		}
		output_profile("\n");
	}

	int i = 0;
	for (; i < noThread; i++) {
		pthread_join(threadA[i], NULL);
	}

	return sync.status;
}

/** Starts the executive test loop
 @return STATUS is SUCCESS if all test passed, FAILED is any test failed.
 **/
STATUS exec_test(struct sync_data *data, /**< the synchronization state data */
int pass, /**< the pass number */
OBJECT *obj) { /**< the current object */
	TIMESTAMP this_t;
	/* check in and out-of-service dates */
	if (global_clock < obj->in_svc)
		this_t = obj->in_svc; /* yet to go in service */
	else if (global_clock <= obj->out_svc)
		this_t = object_sync(obj, global_clock, pass);
	else
		this_t = TS_NEVER; /* already out of service */

	/* check for "soft" event (events that are ignored when stopping) */
	if (this_t < -1)
		this_t = -this_t;
	else if (this_t != TS_NEVER)
		data->hard_event++; /* this counts the number of hard events */

	/* check for stopped clock */
	if (this_t < global_clock) {
		output_error("%s: object %s stopped its clock! (test)", simtime(),
				object_name(obj));
		/* TROUBLESHOOT
		 This indicates that one of the objects in the simulator has encountered a
		 state where it cannot calculate the time to the next state.  This usually
		 is caused by a bug in the module that implements that object's class.
		 */
		data->status = FAILED;
	} else {
		/* check for iteration limit approach */
		if (iteration_counter == 2 && this_t == global_clock) {
			output_verbose("%s: object %s iteration limit imminent", simtime(),
					object_name(obj));
		} else if (iteration_counter == 1 && this_t == global_clock) {
			output_error(
					"convergence iteration limit reached for object %s:%d (test)",
					obj->oclass->name, obj->id);
			/* TROUBLESHOOT
			 This indicates that one of the objects in the simulator has encountered a
			 state where it cannot calculate the time to the next state.  This usually
			 is caused by a bug in the module that implements that object's class.
			 */
		}

		/* manage minimum timestep */
		if (global_minimum_timestep
				> 1&& this_t>global_clock && this_t<TS_NEVER)
			this_t = ((this_t / global_minimum_timestep) + 1)
					* global_minimum_timestep;

		/* if this event precedes next step, next step is now this event */
		if (data->step_to > this_t)
			data->step_to = this_t;
		data->status = SUCCESS;
	}
	return data->status;
}

/**@}*/
