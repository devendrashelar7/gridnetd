#include <iostream>
#include "gldFedAmb.h"
#include "fedtime.hh"
#include "RTI.hh"

using namespace std;

GldFedAmb::GldFedAmb() {
	// initialize all the variable values
	this->federateTime = 0.0;
	this->federateLookahead = 1.0;

	this->isRegulating = false;
	this->isConstrained = false;
	this->isAdvancing = false;
	this->isAnnounced = false;
	this->isReadyToRun = false;

}

GldFedAmb::~GldFedAmb() throw (RTI::FederateInternalError) {
	cout
			<< "************************************************************************"
			<< endl;
	cout << "Created Federation Ambassador" << endl;
	cout
			<< "************************************************************************"
			<< endl;
}

double GldFedAmb::convertTime(const RTI::FedTime& theTime) {
	RTIfedTime castedTime = (RTIfedTime) theTime;
	return castedTime.getTime();
}

///////////////////////////////////////////////////////////////////////////////
/////////////////////// Synchronization Point Callbacks ///////////////////////
///////////////////////////////////////////////////////////////////////////////
void GldFedAmb::synchronizationPointRegistrationSucceeded(const char* label)
		throw (RTI::FederateInternalError) {
	cout << "Devendra : Successfully registered sync point: " << label << endl;
}

void GldFedAmb::synchronizationPointRegistrationFailed(const char *label)
		throw (RTI::FederateInternalError) {
	cout << "Failed to register sync point: " << label << endl;
}

void GldFedAmb::announceSynchronizationPoint(const char *label, const char *tag)
		throw (RTI::FederateInternalError) {
	cout << "Synchronization point announced: " << label << endl;
	if (strcmp(label, "ReadyToRun") == 0)
		this->isAnnounced = true;
}

void GldFedAmb::federationSynchronized(const char *label)
		throw (RTI::FederateInternalError) {
	cout << "Federation Synchronized: " << label << endl;
	if (strcmp(label, "ReadyToRun") == 0)
		this->isReadyToRun = true;
}

///////////////////////////////////////////////////////////////////////////////
//////////////////////////////// Time Callbacks ///////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GldFedAmb::timeRegulationEnabled(const RTI::FedTime& theFederateTime)
		throw (RTI::InvalidFederationTime,
		RTI::EnableTimeRegulationWasNotPending, RTI::FederateInternalError) {
	this->isRegulating = true;
	this->federateTime = convertTime(theFederateTime);
}

void GldFedAmb::timeConstrainedEnabled(const RTI::FedTime& theFederateTime)
		throw (RTI::InvalidFederationTime,
		RTI::EnableTimeConstrainedWasNotPending, RTI::FederateInternalError) {
	this->isConstrained = true;
	this->federateTime = convertTime(theFederateTime);
}

void GldFedAmb::timeAdvanceGrant(const RTI::FedTime& theTime)
		throw (RTI::InvalidFederationTime, RTI::TimeAdvanceWasNotInProgress,
		RTI::FederateInternalError) {
	this->isAdvancing = false;
	this->federateTime = convertTime(theTime);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////// Object Management Callbacks /////////////////////////
///////////////////////////////////////////////////////////////////////////////

//                         //
// Discover Object Methods //
//                         //
void GldFedAmb::discoverObjectInstance(RTI::ObjectHandle theObject,
		RTI::ObjectClassHandle theObjectClass, const char* theObjectName)
				throw (RTI::CouldNotDiscover, RTI::ObjectClassNotKnown,
				RTI::FederateInternalError) {
	cout << "Discoverd Object: handle=" << theObject << ", classHandle="
			<< theObjectClass << ", name=" << theObjectName << endl;
}

//                                 // 
// Reflect Attribute Value Methods //
//                                 // 
void GldFedAmb::reflectAttributeValues(RTI::ObjectHandle theObject,
		const RTI::AttributeHandleValuePairSet& theAttributes,
		const char *theTag) throw (RTI::ObjectNotKnown, RTI::AttributeNotKnown,
				RTI::FederateOwnsAttributes, RTI::FederateInternalError) {
	cout << "Reflection Received:";

	// print the handle
	cout << " object=" << theObject;
	// print the tag
	cout << ", tag=" << theTag;

	// print the attribute information
	cout << ", attributeCount=" << theAttributes.size() << endl;
	for (RTI::ULong i = 0; i < theAttributes.size(); i++) {
		// print the attribute handle
		cout << "\tattrHandle=" << theAttributes.getHandle(i);
		// print the attribute value
		RTI::ULong length = theAttributes.getValueLength(i);
		char *value = theAttributes.getValuePointer(i, length);

		cout << ", attrValue=" << value << endl;
	}
}

void GldFedAmb::reflectAttributeValues(RTI::ObjectHandle theObject,
		const RTI::AttributeHandleValuePairSet& theAttributes,
		const RTI::FedTime& theTime, const char *theTag,
		RTI::EventRetractionHandle theHandle) throw (RTI::ObjectNotKnown,
				RTI::AttributeNotKnown, RTI::FederateOwnsAttributes,
				RTI::InvalidFederationTime, RTI::FederateInternalError) {
	cout << "Reflection Received:";

	// print the handle
	cout << " object=" << theObject;
	// print the tag
	cout << ", tag=" << theTag;
	// print the time
	cout << ", time=" << convertTime(theTime);

	// print the attribute information
	cout << ", attributeCount=" << theAttributes.size() << endl;
	for (RTI::ULong i = 0; i < theAttributes.size(); i++) {
		// print the attribute handle
		cout << "\tattrHandle=" << theAttributes.getHandle(i);
		// print the attribute value
		RTI::ULong length = theAttributes.getValueLength(i);
		char *value = theAttributes.getValuePointer(i, length);

		cout << ", attrValue=" << value << endl;
	}
}

//                             //
// Receive Interaction Methods //
//                             //
void GldFedAmb::receiveInteraction(RTI::InteractionClassHandle theInteraction,
		const RTI::ParameterHandleValuePairSet& theParameters,
		const char *theTag) throw (RTI::InteractionClassNotKnown,
				RTI::InteractionParameterNotKnown, RTI::FederateInternalError) {
	cout << "Interaction Received:";

	// print the handle
	cout << " handle=" << theInteraction;
	// print the tag
	cout << ", tag=" << theTag;

	// print the attribute information
	cout << ", parameterCount=" << theParameters.size() << endl;
	for (RTI::ULong i = 0; i < theParameters.size(); i++) {
		// print the parameter handle
		cout << "\tparamHandle=" << theParameters.getHandle(i);
		// print the parameter value
		RTI::ULong length = theParameters.getValueLength(i);
		char *value = theParameters.getValuePointer(i, length);

		cout << ", paramValue=" << value << endl;
	}
}

void GldFedAmb::receiveInteraction(RTI::InteractionClassHandle theInteraction,
		const RTI::ParameterHandleValuePairSet& theParameters,
		const RTI::FedTime& theTime, const char *theTag,
		RTI::EventRetractionHandle theHandle)
				throw (RTI::InteractionClassNotKnown,
				RTI::InteractionParameterNotKnown, RTI::InvalidFederationTime,
				RTI::FederateInternalError) {
	cout << "Interaction Received:";

	// print the handle
	cout << " handle=" << theInteraction;
	// print the tag
	cout << ", tag=" << theTag;
	// print the time 
	cout << ", time=" << convertTime(theTime);

	// print the attribute information
	cout << ", parameterCount=" << theParameters.size() << endl;
	for (RTI::ULong i = 0; i < theParameters.size(); i++) {
		// print the parameter handle
		cout << "\tparamHandle=" << theParameters.getHandle(i);
		// print the parameter value
		RTI::ULong length = theParameters.getValueLength(i);
		char *value = theParameters.getValuePointer(i, length);

		cout << ", paramValue=" << value << endl;
	}
}

//                       //
// Remove Object Methods //
//                       //
void GldFedAmb::removeObjectInstance(RTI::ObjectHandle theObject,
		const char *theTag) throw (RTI::ObjectNotKnown,
				RTI::FederateInternalError) {
	cout << "Object Removed: handle=" << theObject << endl;
}

void GldFedAmb::removeObjectInstance(RTI::ObjectHandle theObject,
		const RTI::FedTime& theTime, const char *theTag,
		RTI::EventRetractionHandle theHandle) throw (RTI::ObjectNotKnown,
				RTI::InvalidFederationTime, RTI::FederateInternalError) {
	cout << "Object Removed: handle=" << theObject << endl;
}

void GldFedAmb::printObjectInstance() {
	cout << "Printing Object Instance=" << endl;
}

EXPORT_C void gldFedAmb_print() {
	cout << "Hello World" << endl;
}

EXPORT_C GldFedAmb* gldFedAmb_new() {
	return new GldFedAmb();
}

EXPORT_C void printObjectInstance(GldFedAmb * gldFedAmb) {
	gldFedAmb->printObjectInstance();
}

EXPORT_C RTIambassador* rtiAmbassador_new() {
	return new RTI::RTIambassador();
}

void createFederationExecution(RTIambassador* rtiAmb, const char *executionName,
		const char *FED) {
	try {
		rtiAmb->createFederationExecution(executionName, FED);
	} catch (RTI::FederationExecutionAlreadyExists exists) {
		cout << "Didn't create federation, it already existed" << endl;
	}
}

void joinFederationExecution(RTIambassador* rtiAmb, const char *yourName, // supplied C4
		const char *executionName,               // supplied C4
		FederateAmbassador * federateAmbassadorReference) // supplied C1
		{

	rtiAmb->joinFederationExecution(yourName, executionName,
			federateAmbassadorReference);
}

void initializeHandles(RTIambassador * rtiAmb) {
	aHandle = rtiAmb->getObjectClassHandle("ObjectRoot.A");
	aaHandle = rtiAmb->getAttributeHandle("aa", aHandle);
	xHandle = rtiAmb->getInteractionClassHandle("InteractionRoot.X");
	xaHandle = rtiAmb->getParameterHandle("xa", xHandle);

}

void registerFederationSynchronizationPoint(RTIambassador * rtiAmb,
		const char *label, const char *theTag) {
	rtiAmb->registerFederationSynchronizationPoint(label, theTag);
}

bool tick(RTIambassador* rtiAmb) {
	return rtiAmb->tick();
}

void waitForUser() {
	cout << " >>>>>>>>>> Press Enter to Continue <<<<<<<<<<" << endl;
	string line;
	getline(cin, line);
}

void syncAndWait(RTIambassador  * rtiAmb,GldFedAmb * gldFedAmb, const char *label){
	rtiAmb->synchronizationPointAchieved(label);
	cout << "Achieved sync point: " << label
				<< ", waiting for federation..." << endl;
		while (gldFedAmb->isReadyToRun == false) {
			rtiAmb->tick();
		}
}

bool isAnnounced(GldFedAmb * gldFedAmb) {
	return gldFedAmb->isAnnounced;
}

void publishAndSubscribe(RTIambassador * rtiAmb) {
	////////////////////////////////////////////
	// publish all attributes of ObjectRoot.A //
	////////////////////////////////////////////
	// before we can register instance of the object class ObjectRoot.A and
	// update the values of the various attributes, we need to tell the RTI
	// that we intend to publish this information

	// package the information into a handle set
	RTI::AttributeHandleSet *attributes =
			RTI::AttributeHandleSetFactory::create(1);
	attributes->add(aaHandle);

	// do the actual publication
	rtiAmb->publishObjectClass(aHandle, *attributes);

	/////////////////////////////////////////////////
	// subscribe to all attributes of ObjectRoot.A //
	/////////////////////////////////////////////////
	// we also want to hear about the same sort of information as it is
	// created and altered in other federates, so we need to subscribe to it
	rtiAmb->subscribeObjectClassAttributes(aHandle, *attributes);

	/////////////////////////////////////////////////////
	// publish the interaction class InteractionRoot.X //
	/////////////////////////////////////////////////////
	// we want to send interactions of type InteractionRoot.X, so we need
	// to tell the RTI that we're publishing it first. We don't need to
	// inform it of the parameters, only the class, making it much simpler

	// do the publication
	rtiAmb->publishInteractionClass(xHandle);

	////////////////////////////////////////////////////
	// subscribe to the InteractionRoot.X interaction //
	////////////////////////////////////////////////////
	// we also want to receive other interaction of the same type that are
	// sent out by other federates, so we have to subscribe to it first
	rtiAmb->subscribeInteractionClass(xHandle);

	// clean up
	delete attributes;
}

RTI::ObjectHandle registerObject(RTIambassador * rtiAmb) {
	return rtiAmb->registerObjectInstance(
			rtiAmb->getObjectClassHandle("ObjectRoot.A"));
}

double getFederateTime(GldFedAmb * gldFedAmb) {
	return gldFedAmb->federateTime;
}

void updateAttributeValues(RTIambassador * rtiAmb, GldFedAmb * gldFedAmb,
		ObjectHandle objectHandle) {
	RTI::AttributeHandleValuePairSet * attributes =
			RTI::AttributeSetFactory::create(1);

	char aaValue[16];
	sprintf(aaValue, "aa:%f", getLbts(gldFedAmb));

	attributes->add(aaHandle, aaValue, (RTI::ULong) strlen(aaValue) + 1);
	//////////////////////////
	// do the actual update //
	//////////////////////////
	rtiAmb->updateAttributeValues(objectHandle, *attributes, "hi!");

	// note that if you want to associate a particular timestamp with the
	// update. here we send another update, this time with a timestamp:
	RTIfedTime time = gldFedAmb->federateTime + gldFedAmb->federateLookahead;
	rtiAmb->updateAttributeValues(objectHandle, *attributes, time, "hi!");

	// clean up
	delete attributes;
}

//void reflectAttributeValues(RTIambassador * rtiAmb, GldFedAmb * gldFedAmb,
//		ObjectHandle objectHandle) {
//	///////////////////////////////////////////////
//	// create the necessary container and values //
//	///////////////////////////////////////////////
//	// create the collection to store the values in, as you can see
//	// this is quite a lot of work
//	//RTI::AttributeHandleSet *attributes = RTI::AttributeHandleSetFactory::create( 1 );
//	RTI::AttributeHandleValuePairSet *attributes =
//			RTI::AttributeSetFactory::create(1);
//
//	// generate the new values
//	// we use EncodingHelpers to make things nice friendly for both Java and C++
//	char aaValue[16], abValue[16], acValue[16];
//	char * buff;
//	//attributes->add( aaHandle);
//	attributes->add(aaHandle, aaValue, (RTI::ULong) strlen(aaValue) + 1);
//
//	// do the actual reflection
//	//rtiamb->requestObjectAttributeValueUpdate(objectHandle, *attributes);
//	gldFedAmb->reflectAttributeValues(objectHandle, *attributes, "hi!");
//	delete attributes;
//	/*char * buff;
//	 long unsigned int len = 16;
//	 long unsigned int index = 1;
//	 attributes->getValue(index, buff, &len);
//
//	 cout << "Reflection received for aaValue " << buff << endl;*/
//}



EXPORT_C double getLbts(GldFedAmb * gldFedAmb) {
	return gldFedAmb->federateTime;
}

/**
 * This method will send out an interaction of the type InteractionRoot.X. Any
 * federates which are subscribed to it will receive a notification the next time
 * they tick(). Here we are passing only two of the three parameters we could be
 * passing, but we don't actually have to pass any at all!
 */
EXPORT_C void sendInteraction(RTIambassador * rtiAmb, GldFedAmb * gldFedAmb) {
	///////////////////////////////////////////////
	// create the necessary container and values //
	///////////////////////////////////////////////
	// create the collection to store the values in
	RTI::ParameterHandleValuePairSet *parameters =
			RTI::ParameterSetFactory::create(1);

	// generate the new values
	char xaValue[16];
	sprintf(xaValue, "xa:%f", getLbts(gldFedAmb));

	parameters->add(xaHandle, xaValue, (RTI::ULong) strlen(xaValue) + 1);

	//////////////////////////
	// send the interaction //
	//////////////////////////
	rtiAmb->sendInteraction(xHandle, *parameters, "hi!");

	// if you want to associate a particular timestamp with the
	// interaction, you will have to supply it to the RTI. Here
	// we send another interaction, this time with a timestamp:
	RTIfedTime time = gldFedAmb->federateTime + gldFedAmb->federateLookahead;
	rtiAmb->sendInteraction( xHandle, *parameters, time, "hi!" );

	// clean up
	delete parameters;
}

/**
 * This method will request a time advance to the current time, plus the given
 * timestep. It will then wait until a notification of the time advance grant
 * has been received.
 */
void advanceTime(RTIambassador* rtiAmb, GldFedAmb* gldFedAmb, double timestep) {
	// request the advance
	gldFedAmb->isAdvancing = true;
	RTIfedTime newTime = (gldFedAmb->federateTime + timestep);
	rtiAmb->timeAdvanceRequest(newTime);

	// wait for the time advance to be granted. ticking will tell the
	// LRC to start delivering callbacks to the federate
	while (gldFedAmb->isAdvancing) {
		rtiAmb->tick();
	}
}

void cleanUp(RTIambassador* rtiAmb, GldFedAmb* gldFedAmb, char * federationName,
		ObjectHandle objectHandle) {
	//////////////////////////////////////
	// 10. delete the object we created //
	//////////////////////////////////////
	deleteObject(rtiAmb, objectHandle);
	cout << "Deleted Object, handle=" << objectHandle << endl;

	////////////////////////////////////
	// 11. resign from the federation //
	////////////////////////////////////
	rtiAmb->resignFederationExecution(RTI::NO_ACTION);
	cout << "Resigned from Federation" << endl;

	////////////////////////////////////////
	// 12. try and destroy the federation //
	////////////////////////////////////////
	// NOTE: we won't die if we can't do this because other federates
	//       remain. in that case we'll leave it for them to clean up
	try {
		rtiAmb->destroyFederationExecution(federationName);
		cout << "Destroyed Federation" << endl;
	} catch (RTI::FederationExecutionDoesNotExist dne) {
		cout << "No need to destroy federation, it doesn't exist" << endl;
	} catch (RTI::FederatesCurrentlyJoined fcj) {
		cout << "Didn't destroy federation, federates still joined" << endl;
	}

	//////////////////
	// 13. clean up //
	//////////////////
	//delete this->rtiamb;
}

void deleteObject(RTIambassador* rtiAmb, ObjectHandle objectHandle) {
	rtiAmb->deleteObjectInstance(objectHandle, NULL);
}

