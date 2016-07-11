//File RTI.hh

#ifndef RTI_hh
#define RTI_hh

#if defined(_WIN32)
// disable warning about exceptions not being part of a method's signature
#pragma warning(disable: 4290)
// disable warnings about deriving a "dllexport" class from a regular class
#pragma warning(disable: 4275)
#pragma warning(disable: 4251)
#if defined(BUILDING_RTI)
// define the proper qualifiers to import/export symbols from/to DLL
#define RTI_EXPORT __declspec(dllexport)
#else // !BUILDING_RTI
#endif // BUILDING_RTI
// define the proper qualifiers to import/export symbols from/to DLL
#define RTI_EXPORT_FEDTIME __declspec(dllexport)
#else // !BUILDING_RTI
#endif // BUILDING_FEDTIME
#define RTI_EXPORT
#define RTI_EXPORT_FEDTIME
#endif

#ifdef __cplusplus

//
// This was modified for the RTI 1.3NG to allow the use the Standard C++ fstream
// header file or to use of the legacy fstream.h header file.  The issue
// concerns whether ostream is in the global namespace or in namespace std.
//
#ifdef RTI_USES_STD_FSTREAM
#include <fstream>
#define RTI_STD std
#else
#include <fstream>
#define RTI_STD
#endif

/* nothing */
//
// This has been commented out for the RTI 1.3NG due to an inconvenient naming
// collision between struct exception {}; in math.h and the common 
// implementation of the standard C++ exception class with the Sun 4.2 C++ 
// compiler
//
// #include <math.h>
struct RTIambPrivateRefs;
struct RTIambPrivateData;

class RTI {

public:
#include "baseTypes.hh"
#include "RTItypes.hh"

	class RTI_EXPORT RTIambassador {
	public:
#include "RTIambServices.hh"
		RTIambPrivateData* privateData;
	private:
		RTIambPrivateRefs* privateRefs;
	};

	class RTI_EXPORT FederateAmbassador {
	public:
#include "federateAmbServices.hh"
	};
};

//
// RTI_STD was added for the RTI 1.3NG to allow the use of the Standard C++ 
// ostream or to use the legacy ostream.  The issue concerns whether ostream
// is in the global namespace or in namespace std.
//
//  Moved from basetypes.hh to this file in order to avoid the 'using' statement
//  for these operators

//typedef RTI_EXPORT RTI::Exception RTIException;

std::ostream RTI_EXPORT &
operator <<(std::ostream &, RTI::Exception *);

std::ostream RTI_EXPORT &
operator <<(std::ostream &, RTI::Exception const &);

typedef RTI_EXPORT RTI::RTIambassador RTIambassador;
typedef RTI_EXPORT RTI::FederateAmbassador FederateAmbassador;



#else
	typedef struct RTI_EXPORT RTIambassador RTIambassador;
	typedef struct RTI_EXPORT FederateAmbassador FederateAmbassador;

#endif
#endif

