#ifndef EXAMPLEFEDAMB_H_
#define EXAMPLEFEDAMB_H_

#include <RTI.hh>
#include <NullFederateAmbassador.hh>

#ifdef __cplusplus
#define EXPORT_C extern "C"
#else
#define EXPORT_C
#endif

#ifdef __cplusplus

class GldFedAmb : public NullFederateAmbassador
{
public:
	// variables //
	double federateTime;
	double federateLookahead;

	bool isRegulating;
	bool isConstrained;
	bool isAdvancing;
	bool isAnnounced;
	bool isReadyToRun;

	//static vector<RTI::ObjectClassHandle> nodeObjectClassHandles;

	// methods //
	GldFedAmb();
	virtual ~GldFedAmb() throw( RTI::FederateInternalError );

	///////////////////////////////////
	// synchronization point methods //
	///////////////////////////////////
	virtual void synchronizationPointRegistrationSucceeded( const char *label )
	throw( RTI::FederateInternalError );

	virtual void synchronizationPointRegistrationFailed( const char *label )
	throw( RTI::FederateInternalError );

	virtual void announceSynchronizationPoint( const char *label, const char *tag )
	throw( RTI::FederateInternalError );

	virtual void federationSynchronized( const char *label )
	throw( RTI::FederateInternalError );

	//////////////////////////
	// time related methods //
	//////////////////////////
	virtual void timeRegulationEnabled( const RTI::FedTime& theFederateTime )
	throw( RTI::InvalidFederationTime,
			RTI::EnableTimeRegulationWasNotPending,
			RTI::FederateInternalError );

	virtual void timeConstrainedEnabled( const RTI::FedTime& theFederateTime )
	throw( RTI::InvalidFederationTime,
			RTI::EnableTimeConstrainedWasNotPending,
			RTI::FederateInternalError );

	virtual void timeAdvanceGrant( const RTI::FedTime& theTime )
	throw( RTI::InvalidFederationTime,
			RTI::TimeAdvanceWasNotInProgress,
			RTI::FederateInternalError );

	///////////////////////////////
	// object management methods //
	///////////////////////////////
	virtual void discoverObjectInstance( RTI::ObjectHandle theObject,
			RTI::ObjectClassHandle theObjectClass,
			const char* theObjectName )
	throw( RTI::CouldNotDiscover,
			RTI::ObjectClassNotKnown,
			RTI::FederateInternalError );

	virtual void reflectAttributeValues( RTI::ObjectHandle theObject,
			const RTI::AttributeHandleValuePairSet& theAttributes,
			const RTI::FedTime& theTime,
			const char *theTag,
			RTI::EventRetractionHandle theHandle)
	throw( RTI::ObjectNotKnown,
			RTI::AttributeNotKnown,
			RTI::FederateOwnsAttributes,
			RTI::InvalidFederationTime,
			RTI::FederateInternalError );

	virtual void reflectAttributeValues( RTI::ObjectHandle theObject,
			const RTI::AttributeHandleValuePairSet& theAttributes,
			const char *theTag )
	throw( RTI::ObjectNotKnown,
			RTI::AttributeNotKnown,
			RTI::FederateOwnsAttributes,
			RTI::FederateInternalError );

	virtual void receiveInteraction( RTI::InteractionClassHandle theInteraction,
			const RTI::ParameterHandleValuePairSet& theParameters,
			const RTI::FedTime& theTime,
			const char *theTag,
			RTI::EventRetractionHandle theHandle )
	throw( RTI::InteractionClassNotKnown,
			RTI::InteractionParameterNotKnown,
			RTI::InvalidFederationTime,
			RTI::FederateInternalError );

	virtual void receiveInteraction( RTI::InteractionClassHandle theInteraction,
			const RTI::ParameterHandleValuePairSet& theParameters,
			const char *theTag )
	throw( RTI::InteractionClassNotKnown,
			RTI::InteractionParameterNotKnown,
			RTI::FederateInternalError );

	virtual void removeObjectInstance( RTI::ObjectHandle theObject,
			const RTI::FedTime& theTime,
			const char *theTag,
			RTI::EventRetractionHandle theHandle)
	throw( RTI::ObjectNotKnown,
			RTI::InvalidFederationTime,
			RTI::FederateInternalError );

	virtual void removeObjectInstance( RTI::ObjectHandle theObject, const char *theTag )
	throw( RTI::ObjectNotKnown, RTI::FederateInternalError );

	void printObjectInstance();

	/////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////// Private Section ////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////
private:
	double convertTime( const RTI::FedTime& theTime );

};

RTI::ObjectClassHandle aHandle;
RTI::AttributeHandle aaHandle;
RTI::InteractionClassHandle xHandle;
RTI::ParameterHandle xaHandle;

typedef RTI::ObjectHandle ObjectHandle;
typedef RTI::AttributeHandleValuePairSet AttributeHandleValuePairSet;
typedef RTI::FedTime FedTime;
typedef RTI::EventRetractionHandle EventRetractionHandle;


#else
typedef struct GldFedAmb GldFedAmb;
typedef struct FederateHandle FederateHandle;
typedef unsigned long int ULong;
typedef ULong ObjectHandle;
typedef double RTIfedTime;
//typedef struct FederateAmbassador FederateAmbassador;
#endif /*EXAMPLEFEDAMB_H_*/
#endif

EXPORT_C GldFedAmb* gldFedAmb_new();

EXPORT_C void gldFedAmb_print();

EXPORT_C void printObjectInstance(GldFedAmb* gldFedAmb);

EXPORT_C RTIambassador* rtiAmbassador_new();

EXPORT_C void createFederationExecution(RTIambassador* rtiAmb,
		const char *executionName, const char *FED);

EXPORT_C void joinFederationExecution(RTIambassador* rtiAmb,
		const char *yourName, const char *executionName,
		FederateAmbassador * federateAmbassadorReference);

EXPORT_C void initializeHandles(RTIambassador * rtiAmb);

EXPORT_C void registerFederationSynchronizationPoint(RTIambassador * rtiAmb,
		const char *label, const char *theTag);

EXPORT_C bool tick(RTIambassador* rtiAmb);

EXPORT_C void waitForUser();

EXPORT_C void syncAndWait(RTIambassador  * rtiAmb,GldFedAmb * gldFedAmb, const char *label);

EXPORT_C bool isAnnounced(GldFedAmb * gldFedAmb);

EXPORT_C void publishAndSubscribe(RTIambassador * rtiAmb);

EXPORT_C unsigned long int registerObject(RTIambassador * rtiAmb);

EXPORT_C double getFederateTime(GldFedAmb * gldFedAmb);

EXPORT_C void updateAttributeValues(RTIambassador * rtiAmb,
		GldFedAmb * gldFedAmb, ObjectHandle objectHandle);

EXPORT_C void reflectAttributeValues(RTIambassador * rtiAmb,
		GldFedAmb * gldFedAmb, ObjectHandle objectHandle);

//EXPORT_C void reflectAttributeValues(ObjectHandle theObject,
//		const AttributeHandleValuePairSet& theAttributes,
//		const FedTime& theTime, const char *theTag,
//		EventRetractionHandle theHandle) ;
//
//EXPORT_C void reflectAttributeValues(ObjectHandle theObject,
//		const AttributeHandleValuePairSet& theAttributes,
//		const char *theTag) ;

EXPORT_C double getLbts(GldFedAmb * gldFedAmb);

EXPORT_C void sendInteraction(RTIambassador * rtiAmb, GldFedAmb * gldFedAmb);

EXPORT_C void advanceTime(RTIambassador* rtiAmb, GldFedAmb* gldFedAmb,
		double timestep);

EXPORT_C void cleanUp(RTIambassador* rtiAmb, GldFedAmb* gldFedAmb,
		char * federationName, ObjectHandle objectHandle);

EXPORT_C void deleteObject(RTIambassador* rtiAmb, ObjectHandle objectHandle);
