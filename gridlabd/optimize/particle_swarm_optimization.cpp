/** $Id: particle_swarm_optimization.cpp 1182 2008-12-22 22:08:36Z bvyakaranam $
	Copyright (C) 2008 Battelle Memorial Institute
	@file particle_swarm_optimization.cpp.cpp
	@addtogroup optimize
	@ingroup optimize

	The particle_swarm_optimization object implements the Particle Swarm Optimization and it can easily be applied for
	single and multi-object optimization problems.

 **/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <iostream>

#include "gridlabd.h"
#include "particle_swarm_optimization.h"

CLASS* particle_swarm_optimization::oclass = NULL;
CLASS* particle_swarm_optimization::pclass = NULL;
particle_swarm_optimization *particle_swarm_optimization::defaults = NULL;

static PASSCONFIG passconfig = PC_PRETOPDOWN|PC_POSTTOPDOWN;
static PASSCONFIG clockpass = PC_POSTTOPDOWN;

// comparison operators 
static inline bool lt(double a, double b) { return a<b; };
static inline bool le(double a, double b) { return a<=b; };
static inline bool eq(double a, double b) { return a==b; };
static inline bool ne(double a, double b) { return a!=b; };
static inline bool ge(double a, double b) { return a>=b; };
static inline bool gt(double a, double b) { return a>b; };

// Class registration is only called once to register the class with the core
particle_swarm_optimization::particle_swarm_optimization(MODULE *module)
{
	if (oclass==NULL)
	{
		oclass = gl_register_class(module,"particle_swarm_optimization",sizeof(particle_swarm_optimization),passconfig);
		if (oclass==NULL)
			throw "unable to register class particle_swarm_optimization";

		if (gl_publish_variable(oclass,
			PT_char1024, "objective", PADDR(objective), PT_DESCRIPTION, "Optimization objective value",
			PT_char1024, "variable_1", PADDR(variable_1), PT_DESCRIPTION, "First optimization decision variable",
			PT_char1024, "variable_2", PADDR(variable_2), PT_DESCRIPTION, "Second optimization decision variable",
			PT_char1024, "variable_3", PADDR(variable_3), PT_DESCRIPTION, "Third optimization decision variable",
			PT_char1024, "constraint1", PADDR(constraint1), PT_DESCRIPTION, "Optimization constraints",
			PT_char1024, "constraint2", PADDR(constraint2), PT_DESCRIPTION, "Optimization constraints",
			PT_char1024, "constraint3", PADDR(constraint3), PT_DESCRIPTION, "Optimization constraints",
			PT_char1024, "constraint4", PADDR(constraint4), PT_DESCRIPTION, "Optimization constraints",
			PT_char1024, "constraint5", PADDR(constraint5), PT_DESCRIPTION, "Optimization constraints",
			PT_char1024, "constraint6", PADDR(constraint6), PT_DESCRIPTION, "Optimization constraints",
			PT_char1024, "constraint7", PADDR(constraint7), PT_DESCRIPTION, "Optimization constraints",
			PT_char1024, "constraint8", PADDR(constraint8), PT_DESCRIPTION, "Optimization constraints",
			
			PT_double, "no_particles", PADDR(no_particles), PT_DESCRIPTION, "The number of agents (particles) in a swarm.", //this might not work if it isn't specified in input
			PT_double, "C1", PADDR(C1), PT_DESCRIPTION, "Hookes’s coefficients", //
			PT_double, "C2", PADDR(C2), PT_DESCRIPTION, "Hookes’s coefficients", //
			PT_double, "no_unknowns", PADDR(no_unknowns), PT_DESCRIPTION, "This is the dimension of solution space. It is equal to the number of unknowns in a variable to be optimized", //
			PT_double, "max_iterations", PADDR(max_iterations), PT_DESCRIPTION, "Total number of iterations.",

			PT_double, "w", PADDR(w), PT_DESCRIPTION, "Weighting factor.",
			
			PT_double, "obj_sol", PADDR(obj_sol), PT_DESCRIPTION, "Weighting factor.",

			//PT_double, "variable_1", PADDR(variable_1), PT_DESCRIPTION, "variable_1 value", //
			//PT_double, "variable_2", PADDR(variable_2), PT_DESCRIPTION, "variable_2 value", //
			//PT_double, "variable_3", PADDR(variable_3), PT_DESCRIPTION, "variable_3 value", //

			PT_double, "gbest", PADDR(gbest), PT_DESCRIPTION, "gbest", //
			PT_double, "gbest1", PADDR(gbest1), PT_DESCRIPTION, "gbest1", //
			PT_double, "gbest2", PADDR(gbest2), PT_DESCRIPTION, "gbest2", //
			PT_double, "gbest3", PADDR(gbest3), PT_DESCRIPTION, "gbest3", //
			
			PT_double, "position_lb", PADDR(position_lb), PT_DESCRIPTION, "position_lb", //
			PT_double, "position_ub", PADDR(position_ub), PT_DESCRIPTION, "position_ub", //
			
			PT_double, "velocity_lb", PADDR(velocity_lb), PT_DESCRIPTION, "velocity_lb", //
			PT_double, "velocity_ub", PADDR(velocity_ub), PT_DESCRIPTION, "velocity_ub", //

			PT_double,"cycle_interval[s]", PADDR(cycle_interval),
			PT_int32, "trials", PADDR(trials), PT_DESCRIPTION, "Limits on number of trials", //this might not work if it isn't specified in input
			PT_enumeration, "goal", PADDR(goal), PT_DESCRIPTION, "Optimization objective goal",
				PT_KEYWORD, "EXTREMUM", OG_EXTREMUM,
				PT_KEYWORD, "MINIMUM", OG_MINIMUM,
				PT_KEYWORD, "MAXIMUM", OG_MAXIMUM,
			PT_enumeration, "optimizer", PADDR(optimizer), PT_DESCRIPTION, "Optimization type",

			PT_enumeration,"mode", PADDR(mode),
			PT_KEYWORD,"MINIMUM",MINIMUM,
			PT_KEYWORD,"MAXIMUM",MAXIMUM,

				//DISCRETE checks each surrounding value and picks the best value after checking once

			NULL)<1) GL_THROW("unable to publish properties in %s",__FILE__);

		//defaults = this;
		//memset(this,0,sizeof(simple));

		defaults = this;
		memset(this,0,sizeof(particle_swarm_optimization));
	}
}


int particle_swarm_optimization::isa(char *classname)
{
	return strcmp(classname,"particle_swarm_optimization")==0;
}

// Object creation is called once for each object that is created by the core
int particle_swarm_optimization::create(void)
{
	/*int rval;*/
	cycle_interval_TS = 0;
	time_cycle_interval = 0;	
	///*
	//retur*/n rval;
	return 1; // return 1 on success, 0 on failure
}

// Object initialization is called once after all object have been created
int particle_swarm_optimization::init(OBJECT *parent)
{
	//int rval;
	OBJECT *obj = OBJECTHDR(this);
	char buffer[1024];
	struct {
		char *name;
		char *param;
		double **var;
		bool (**op)(double,double);
		double *value;
	} map[] = {
		{"objective",	objective,	&pObjective},
		{"variable_1",	variable_1,	&pVariable1},
		{"variable_2",	variable_2,	&pVariable2},
		{"variable_3",	variable_3,	&pVariable3},
		//8 possible constraints to allow for upper and lower bounds for 3 decision variables and objective function
		{"constraint1",	constraint1, &pConstraint1, &(constrain1.op), &(constrain1.value)},
		{"constraint2",	constraint2, &pConstraint2, &(constrain2.op), &(constrain2.value)},
		{"constraint3",	constraint3, &pConstraint3, &(constrain3.op), &(constrain3.value)},
		{"constraint4",	constraint4, &pConstraint4, &(constrain4.op), &(constrain4.value)},
		{"constraint5",	constraint5, &pConstraint5, &(constrain5.op), &(constrain5.value)},
		{"constraint6",	constraint6, &pConstraint6, &(constrain6.op), &(constrain6.value)},
		{"constraint7",	constraint7, &pConstraint7, &(constrain7.op), &(constrain7.value)},
		{"constraint8",	constraint8, &pConstraint8, &(constrain8.op), &(constrain8.value)},
	};
	int n;
	trail_run =0;
	for ( n=0 ; n<sizeof(map)/sizeof(map[0]) ; n++ )
	{
		char oname[1024];
		char pname[1024];
		OBJECT *obj;
		PROPERTY *prop;
		if ( strcmp(map[n].param,"")==0 )
			continue;
		if ( sscanf(map[n].param,"%[^.:].%[a-zA-Z0-9_.]",oname,pname)!=2 )
		{
			gl_error("%s could not be parsed, expected term in the form 'objectname'.'propertyname'", map[n].name);
			return 0;
		}
		//if ( obj->rank>=obj->rank ) gl_set_rank(obj,obj->rank-1);
		 //find the object
		obj = gl_get_object(oname);
		if ( obj==NULL )
		{
			gl_error("object '%s' could not be found", oname);
			return 0;
		}
		
		//if ( optimizer == OT_DISCRETE || optimizer == OT_DISCRETE_ITERATE || optimizer == OT_TEST )
		//{
			//This makes it update between pre-sync and post-sync, but not between post-sync and pre-sync (updates during sync because bottom up)
			//if ( my->rank>=obj->rank ) gl_set_rank(my,obj->rank-1);
		//}

		// get property
		prop = gl_get_property(obj,pname);
		if ( prop==NULL )
		{
			gl_error("property '%s' could not be found in object '%s'", pname, oname);
			return 0;
		}

		if ( prop->ptype == PT_double )
		{
			*(map[n].var) = (double*)gl_get_addr(obj,pname);	
		}
		
		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// NOTE: Only real part of complex values (both obj. function and variables) are used in optimization currently. //
		// Add option for user to either convert to real part or magnitude later.										 //
		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		else if ( prop->ptype == PT_complex )
		{
			*(map[n].var) = &(((complex*)gl_get_addr(obj,pname))->Re());
		}
		else
		{
			gl_error("property '%s' in object '%s' must be a double or complex", pname, oname);
			return 0;
		}


		// parse constraints
		if ( map[n].op!=NULL )
		{
			char varname[256], op[32], value[64];

			switch (sscanf(map[n].param, "%[^ \t<=>!]%[<=>!]%s", varname, op, value))
			{
				case 0:
					break;
				case 1:
					gl_error("Constraint '%s' in object '%s' is missing a valid comparison operator", map[n].param, oname);
					return 0;
				case 2:
					gl_error("Constraint '%s' in object '%s' is missing a valid comparison value", map[n].param, oname);
					return 0;
				case 3:
					{	if (strcmp(op,"<")==0) *(map[n].op) = lt; else
						if (strcmp(op,"<=")==0) *(map[n].op) = le; else
						if (strcmp(op,"==")==0) *(map[n].op) = eq; else
						if (strcmp(op,">=")==0) *(map[n].op) = ge; else
						if (strcmp(op,">")==0) *(map[n].op) = gt; else
						if (strcmp(op,"!=")==0) *(map[n].op) = ne; else
						{
							gl_error("Constraint '%s' in object '%s' operator '%s' is invalid", map[n].param, oname, op);
							return 0;
						}
					}
					//Convert string to double
					////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
					// NOTE: The following line will convert from a string to double and ignore any imaginary parts.				  //
					// This line will need to be changed if the user wants to convert from complex to magnitude instead of real part. //
					// Or the user can just input the desired magnitude as the RHS of the constraint rather and 
					////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
					*(map[n].value) = atof(value);
					break;
				default:
					gl_error("Constraint '%s' in object '%s' parsing resulted in an internal error", map[n].param, oname);
					return 0;
			}
		}
	}


	if (cycle_interval == 0.0)
	cycle_interval = 900.0;
	if (cycle_interval <= 0)
	{
		GL_THROW("emissions:%s must have a positive cycle_interval time",obj->name);
		/*  TROUBLESHOOT
		The calculation cycle interval for the emissions object must be a non-zero, positive
		value.  Please specify one and try again.
		*/
	}

	cycle_interval_TS = (TIMESTAMP)(cycle_interval);
		// read the current values
	if(!no_particles)
		no_particles = 100;
		//check the no_particles limit
	else if ( no_particles<1 )
	{
		gl_error("The no_particles limit 'no_particles' in PSO object '%s' must be a positive integer value", gl_name(obj,buffer,sizeof(buffer))?buffer:"???");
		return 0;
	}

	if(!no_unknowns)
	    no_unknowns = 1;
		//check the no_particles limit
	else if ( no_unknowns<1 )
	{
		gl_error("The no_particles limit 'no_unknowns' in PSO object '%s' must be a positive integer value", gl_name(obj,buffer,sizeof(buffer))?buffer:"???");
		return 0;
	}


	if(!max_iterations)
	    max_iterations = 200;
		//check the no_particles limit
	else if ( max_iterations<1 )
	{
		gl_error("The no_particles limit 'max_iterations' in PSO object '%s' must be a positive integer value", gl_name(obj,buffer,sizeof(buffer))?buffer:"???");
		return 0;
	}

	
	//if(!variable_1)
	//    variable_1 = 0;
	//	//check the no_particles limit
	//else if ( variable_1<0 )
	//{
	//	gl_error("The no_particles limit 'variable_1' in PSO object '%s' must be a positive integer value or zero", gl_name(obj,buffer,sizeof(buffer))?buffer:"???");
	//	return 0;
	//}

	if(!C1)
	    C1 = 2;
		//check the no_particles limit
	else if ( C1<1 )
	{
		gl_error("The no_particles limit 'C1' in PSO object '%s' must be a positive integer value", gl_name(obj,buffer,sizeof(buffer))?buffer:"???");
		return 0;
	}

	if(!C2)
	    C2 = 2;
		//check the no_particles limit
	else if ( C2<1 )
	{
		gl_error("The no_particles limit 'C2' in PSO object '%s' must be a positive integer value", gl_name(obj,buffer,sizeof(buffer))?buffer:"???");
		return 0;
	}

	if (pVariable1)
		next_x1 = *pVariable1;
	if (pVariable2)
		next_x2 = *pVariable2;
	if (pVariable3)
		next_x3 = *pVariable3;

	//if(!variable_2)
	//    variable_2 = 0;
	//	//check the no_particles limit
	//else if ( variable_2<0 )
	//{
	//	gl_error("The no_particles limit 'variable_2' in PSO object '%s' must be a positive integer value or zero", gl_name(obj,buffer,sizeof(buffer))?buffer:"???");
	//	return 0;
	//}

	
	//if(!variable_3)
	//    variable_3 = 0;
	//	//check the no_particles limit
	//else if ( variable_3<0 )
	//{
	//	gl_error("The no_particles limit 'variable_3' in PSO object '%s' must be a positive integer value or zero", gl_name(obj,buffer,sizeof(buffer))?buffer:"???");
	//	return 0;
	//}
	//Convert it to a TIMESTAMP
	/*cycle_interval_TS = (TIMESTAMP)(cycle_interval);*/
	//return rval;
	time_cycle_interval = gl_globalclock;

	if (pVariable1)
		gl_verbose("    given %s", variable_1);
	if (pVariable2)
		gl_verbose("    given %s", variable_2);
	if (pVariable3)
		gl_verbose("    given %s", variable_3);

	if (pConstraint1)		
		gl_verbose("    subject to %s",constraint1);
	if (pConstraint2)		
		gl_verbose("    subject to %s",constraint2);
	if (pConstraint3)		
		gl_verbose("    subject to %s",constraint3);
	if (pConstraint4)		
		gl_verbose("    subject to %s",constraint4);
	if (pConstraint5)		
		gl_verbose("    subject to %s",constraint5);
	if (pConstraint6)		
		gl_verbose("    subject to %s",constraint6);
	if (pConstraint7)		
		gl_verbose("    subject to %s",constraint7);
	if (pConstraint8)		
		gl_verbose("    subject to %s",constraint8);
	return 1; // return 1 on success, 0 on failure

}


//Presync is called when the clock needs to advance on the first top-down pass
//For presync and postsync:
//Each case checks all values delta around that case. The last case selects the best of those values.
//If no feasible solution exists, return an error.
//Note that the value from the previous time step will always be tested at the current time step, which is okay because the objective function may
//	change such that this value is once again optimal.
TIMESTAMP particle_swarm_optimization::presync(TIMESTAMP t0, TIMESTAMP t1)
{
	OBJECT *obj = OBJECTHDR(this);

	if (prev_cycle_time != t0)	//New timestamp - accumulate
	{
		//Store current cycle
		curr_cycle_time = t0;
		
		//Update tracking variable
		prev_cycle_time = t0;
	}
	//t_next should only equal 0 when simulation first starts.

	if (curr_cycle_time >= time_cycle_interval && trail_run > 0)	//Update values
	{

			for (int particle = 0; particle < no_particles; particle++) 
			{
				for (int dimension = 0; dimension < no_unknowns; dimension++) 
				{
					particle_position[particle][dimension] = position_lb + (position_ub - position_lb) * gl_random_uniform(0.0,1.0);
					particle_velocity[particle][dimension] = velocity_lb + (velocity_ub - velocity_lb) * gl_random_uniform(0.0,1.0);

				}
			}

			// Initialize the pbest fitness 

				pbset_fitness = -1000.0;
				gbest_value = -1000.0;

		for (int iteration = 0; iteration < max_iterations; iteration++) 
		{
			for (int particle = 0; particle < no_particles; particle++) 

			{
				*pVariable1 = particle_position[particle][0];
				*pVariable2 = particle_position[particle][1];
				*pVariable3 = particle_position[particle][2];

				//next_x1 = particle_position[particle][0];
				//next_x2 = particle_position[particle][1];
				//next_x3 = particle_position[particle][2];
				//
				//*pVariable1 = next_x1;
				//*pVariable2 = next_x2;
				//*pVariable3 = next_x3;
				//

				//solution = *pObjective;
				solution = *pVariable1+*pVariable2+*pVariable3;
				//solution = next_x1+next_x2+next_x3;
				
				//if ((*pVariable1<= 2100) && (*pVariable1>=0) && (*pVariable2<= 2200) && (*pVariable2>=0)&& (*pVariable3<= 2300) && (*pVariable3>=0))

				//if ((*pVariable1 + *pVariable2 + *pVariable3 >= 500) && (*pVariable1>=0) && (*pVariable2>=0) && (*pVariable3>=0))

				//if(pConstraint1 && pConstraint2 && pConstraint3 && pConstraint4 && pConstraint5 && pConstraint6)
				violation = check_var_constraints(); //declared in header

			switch(mode) {

				case MINIMUM:

					if( !violation )

						current_fitness[particle] = -solution;	
					else
						current_fitness[particle] = -solution-1000000000;
					break;

				case MAXIMUM:

					if( !violation )

						current_fitness[particle] = solution;	
					else
						current_fitness[particle] = solution-1000000000;
					break;

				}
			}


			// Decide pbest among all the particles

			for (int particle = 0; particle < no_particles; particle++) 

			{
				if (current_fitness[particle] > pbset_fitness)
				{
					pbset_fitness= current_fitness[particle];
				
				for (int dimension = 0; dimension < no_unknowns; dimension++)
				{
					pbest[0][dimension] = particle_position[particle][dimension];
				}	
				}

			}			

			if (pbset_fitness> gbest_value)
			{
				gbest_value= pbset_fitness;
			for (int dimension = 0; dimension < no_unknowns; dimension++)	
				{
				gbest[0][dimension] = pbest[0][dimension];				
				}
			}

			gbest1 = gbest[0][0]; 
			gbest2 = gbest[0][1];
			gbest3 = gbest[0][2];
			obj_sol = gbest1+gbest2+gbest3;

				// Update position and velocity

			for (int particle = 0; particle < no_particles; particle++) 

			{
				for (int dimension = 0; dimension < no_unknowns; dimension++)
				{
					rand1 = gl_random_uniform(0.0,1.0);
					rand2 = gl_random_uniform(0.0,1.0);

					particle_velocity[particle][dimension] = w*particle_velocity[particle][dimension] + C1 * rand1 * (pbest[0][dimension] - particle_position[particle][dimension])+C2 * rand2 * (gbest[0][dimension] - particle_position[particle][dimension]);

					particle_position[particle][dimension] = particle_position[particle][dimension]+particle_velocity[particle][dimension];

				}
			}
		}

		time_cycle_interval += cycle_interval_TS;
		//t1 = time_cycle_interval;
	}

		trail_run+=1;
	//if (t1 > time_cycle_interval)
	//{
	//	t1 = time_cycle_interval;	//Next output cycle is sooner
	//}

	//Make sure we don't send out a negative TS_NEVER
	//if (t1 == TS_NEVER)
	//	return TS_NEVER;

	//else
	//{
	//	t1_temp = t1;
	//	return -t1_temp;
	//}
	if (time_cycle_interval == TS_NEVER)
		return TS_NEVER;
	else
		return -time_cycle_interval;
}
TIMESTAMP particle_swarm_optimization::sync(TIMESTAMP t0, TIMESTAMP t1) 
{
	TIMESTAMP t2 = TS_NEVER;
	return t2;
}
// Postsync is called when the clock needs to advance on the second top-down pass
TIMESTAMP particle_swarm_optimization::postsync(TIMESTAMP t0, TIMESTAMP t1)
{
	OBJECT *my = OBJECTHDR(this);
	return -(t1+1);	

}
						bool particle_swarm_optimization::constraint_broken(bool (*op)(double,double), double value, double x)
						{
							return !op(x,value);
						}

						//Check constraints on decision variables
						//Constraints on objective function will be checked separately
						bool particle_swarm_optimization::check_var_constraints()
						{
							if( pConstraint1 && pConstraint1 != pObjective)
							{
								if ( constraint_broken(constrain1.op,constrain1.value,*pConstraint1) )
									return true;
							}
							if( pConstraint2  && pConstraint2 != pObjective)
							{
								if ( constraint_broken(constrain2.op,constrain2.value,*pConstraint2) )
									return true;
							} 
							if( pConstraint3  && pConstraint3 != pObjective)
							{
								if ( constraint_broken(constrain3.op,constrain3.value,*pConstraint3) )
									return true;
							}
							if( pConstraint4  && pConstraint4 != pObjective)
							{
								if ( constraint_broken(constrain4.op,constrain4.value,*pConstraint4) )
									return true;
							}
							if( pConstraint5  && pConstraint5 != pObjective)
							{
								if ( constraint_broken(constrain5.op,constrain5.value,*pConstraint5) )
									return true;
							}
							if( pConstraint6  && pConstraint6 != pObjective)
							{
								if ( constraint_broken(constrain6.op,constrain6.value,*pConstraint6) )
									return true;
							} 
							if( pConstraint7  && pConstraint7 != pObjective)
							{
								if ( constraint_broken(constrain7.op,constrain7.value,*pConstraint7) )
									return true;
							}
							if( pConstraint8  && pConstraint8 != pObjective)
							{
								if ( constraint_broken(constrain8.op,constrain8.value,*pConstraint8) )
									return true;
							}	
							return false;
						}

						//Check constraints on objective function
						bool particle_swarm_optimization::check_obj_constraints()
						{
							if( pConstraint1 == pObjective )
							{
								if ( constraint_broken(constrain1.op,constrain1.value,*pConstraint1) )
									return true;
							}
							if( pConstraint2  == pObjective )
							{
								if ( constraint_broken(constrain2.op,constrain2.value,*pConstraint2) )
									return true;
							} 
							if( pConstraint3  == pObjective )
							{
								if ( constraint_broken(constrain3.op,constrain3.value,*pConstraint3) )
									return true;
							}
							if( pConstraint4  == pObjective )
							{
								if ( constraint_broken(constrain4.op,constrain4.value,*pConstraint4) )
									return true;
							}
							if( pConstraint5  == pObjective )
							{
								if ( constraint_broken(constrain5.op,constrain5.value,*pConstraint5) )
									return true;
							}
							if( pConstraint6  == pObjective )
							{
								if ( constraint_broken(constrain6.op,constrain6.value,*pConstraint6) )
									return true;
							} 
							if( pConstraint7  == pObjective )
							{
								if ( constraint_broken(constrain7.op,constrain7.value,*pConstraint7) )
									return true;
							}
							if( pConstraint8  == pObjective )
							{
								if ( constraint_broken(constrain8.op,constrain8.value,*pConstraint8) )
									return true;
							}
							return false;
						}



//////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF CORE LINKAGE
//////////////////////////////////////////////////////////////////////////

EXPORT int create_particle_swarm_optimization(OBJECT **obj, OBJECT *parent)
{
	try
	{
		*obj = gl_create_object(particle_swarm_optimization::oclass);
		if (*obj!=NULL)
		{
			particle_swarm_optimization *my = OBJECTDATA(*obj,particle_swarm_optimization);
			gl_set_parent(*obj,parent);
			return my->create();
		}
	}
	catch (const char *msg)
	{
		gl_error("create_simple: %s", msg);
	}
	return 0;
}

EXPORT int init_particle_swarm_optimization(OBJECT *obj, OBJECT *parent)
{
	try
	{
		if (obj!=NULL){
			return OBJECTDATA(obj,particle_swarm_optimization)->init(parent);
		}
	}
	catch (const char *msg)
	{
		char name[64];
		gl_error("init_particle_swarm_optimization(obj=%s): %s", gl_name(obj,name,sizeof(name)), msg);
		return 0;
	}
	return 1;
}

EXPORT int isa_particle_swarm_optimization(OBJECT *obj, char *classname)
{
	if(obj != 0 && classname != 0){
		return OBJECTDATA(obj,particle_swarm_optimization)->isa(classname);
	} else {
		return 0;
	}
}

EXPORT TIMESTAMP sync_particle_swarm_optimization(OBJECT *obj, TIMESTAMP t1, PASSCONFIG pass)
{
	TIMESTAMP t2 = TS_NEVER;
	particle_swarm_optimization *my = OBJECTDATA(obj,particle_swarm_optimization);
	try
	{
		switch (pass) {
		case PC_PRETOPDOWN:
 			t2 = my->presync(obj->clock,t1);
			break;
		case PC_POSTTOPDOWN:
			t2 = my->postsync(obj->clock,t1);
			break;
		default:
			GL_THROW("invalid pass request (%d)", pass);
			break;
		}
		if (pass==clockpass)
			obj->clock = t1;
	}
	catch (const char *msg)
	{
		char name[64];
		gl_error("sync_particle_swarm_optimization(obj=%s): %s", gl_name(obj,name,sizeof(name)), msg);
		t2 = TS_INVALID;
	}
	return t2;
}