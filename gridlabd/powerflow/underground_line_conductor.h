// $Id: underground_line_conductor.h 1182 2008-12-22 22:08:36Z dchassin $
//	Copyright (C) 2008 Battelle Memorial Institute

#ifndef _UNDERGROUNDLINECONDUCTOR_H
#define _UNDERGROUNDLINECONDUCTOR_H

#include "line.h"

class underground_line_conductor : public powerflow_library
{
public:
	static CLASS *oclass;
	static CLASS *pclass;
public:
	double outer_diameter;		///< Outer diameter of conductor and sheath
	double conductor_gmr;		///< Geometric mean radius of the conductor
	double conductor_diameter;	///< Diameter of conductor
	double conductor_resistance;	///< Resistance of conductor in ohm/mile
	double neutral_gmr;			///< Geometric mean radius of the neutral conductor
	double neutral_diameter;	///< Diameter of the neutral conductor
	double neutral_resistance;	///< Resistance of netural conductor in ohm/mile
	int16  neutral_strands;		///< Number of cable strands in neutral conductor
	double insulation_rel_permitivitty;	///< Relative permitivitty of the insulation around conductor
	double shield_gmr;			///< Geometric mean radius of shielding sheath
	double shield_resistance;	///< Resistance of shielding sheath in ohms/mile
	LINERATINGS winter, summer;	///< Operational amp rating of conductor in summer and winter
	
	underground_line_conductor(MODULE *mod);
	inline underground_line_conductor(CLASS *cl=oclass):powerflow_library(cl){};
	int isa(char *classname);
	int create(void);
	int init(OBJECT *parent);
};

#endif // _UNDERGROUNDLINECONDUCTOR_H
