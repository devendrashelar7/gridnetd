// $Id: meter.h 1182 2008-12-22 22:08:36Z dchassin $
//	Copyright (C) 2008 Battelle Memorial Institute

#ifndef _VOLTDUMP_H
#define _VOLTDUMP_H

#include "powerflow.h"
#include "node.h"

typedef enum {
	VDM_RECT,
	VDM_POLAR
} VDMODE;

class voltdump
{
public:
	TIMESTAMP runtime;	///< the time to check voltage data
	char32 group;		///< the group ID to output data for (all nodes if empty)
	char256 filename;	///< the file to dump the voltage data into
	int32 runcount;		///< the number of times the file has been written to
	enumeration mode;		///< dumps the voltages in either polar or rectangular notation
public:
	static CLASS *oclass;
public:
	voltdump(MODULE *mod);
	int create(void);
	int init(OBJECT *parent);
	int commit(TIMESTAMP t);
	int isa(char *classname);

	void dump(TIMESTAMP t);
};

#endif // _VOLTDUMP_H

