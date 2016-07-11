/** $Id: underground_line.cpp 1182 2008-12-22 22:08:36Z dchassin $
	Copyright (C) 2008 Battelle Memorial Institute
	@file underground_line.cpp
	@addtogroup underground_line 
	@ingroup line

	@{
**/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <iostream>
using namespace std;

#include "line.h"

CLASS* underground_line::oclass = NULL;
CLASS* underground_line::pclass = NULL;

underground_line::underground_line(MODULE *mod) : line(mod)
{
	if(oclass == NULL)
	{
		pclass = line::oclass;
		
		oclass = gl_register_class(mod,"underground_line",sizeof(underground_line),PC_PRETOPDOWN|PC_BOTTOMUP|PC_POSTTOPDOWN|PC_UNSAFE_OVERRIDE_OMIT);
        if(oclass == NULL)
            GL_THROW("unable to register object class implemented by %s",__FILE__);
        
        if(gl_publish_variable(oclass,
			PT_INHERIT, "line",
			NULL) < 1) GL_THROW("unable to publish properties in %s",__FILE__);
			if (gl_publish_function(oclass,	"create_fault", (FUNCTIONADDR)create_fault_ugline)==NULL)
				GL_THROW("Unable to publish fault creation function");
			if (gl_publish_function(oclass,	"fix_fault", (FUNCTIONADDR)fix_fault_ugline)==NULL)
				GL_THROW("Unable to publish fault restoration function");
		;
    }
}

int underground_line::create(void)
{
	int result = line::create();
	return result;
}


int underground_line::init(OBJECT *parent)
{
	double *temp_rating_value = NULL;
	double temp_rating_continuous = 10000.0;
	double temp_rating_emergency = 20000.0;
	char index;
	OBJECT *temp_obj;

	int result = line::init(parent);

	if (!configuration)
		throw "no underground line configuration specified.";
		/*  TROUBLESHOOT
		No underground line configuration was specified.  Please use object line_configuration
		with appropriate values to specify an underground line configuration
		*/

	if (!gl_object_isa(configuration, "line_configuration"))
		throw "invalid line configuration for underground line";
		/*  TROUBLESHOOT
		The object specified as the configuration for the underground line is not a valid
		configuration object.  Please ensure you have a line_configuration object selected.
		*/

	//Test the phases
	line_configuration *config = OBJECTDATA(configuration, line_configuration);

	test_phases(config,'A');
	test_phases(config,'B');
	test_phases(config,'C');
	test_phases(config,'N');

	if ((!config->line_spacing || !gl_object_isa(config->line_spacing, "line_spacing")) && config->impedance11 == 0.0 && config->impedance22 == 0.0 && config->impedance33 == 0.0)
		throw "invalid or missing line spacing on underground line";
		/*  TROUBLESHOOT
		The configuration object for the underground line is missing the line_spacing configuration
		or the item specified in that location is invalid.
		*/

	recalc();

	//Values are populated now - populate link ratings parameter
	for (index=0; index<4; index++)
	{
		if (index==0)
		{
			temp_obj = config->phaseA_conductor;
		}
		else if (index==1)
		{
			temp_obj = config->phaseB_conductor;
		}
		else if (index==2)
		{
			temp_obj = config->phaseC_conductor;
		}
		else	//Must be 3
		{
			temp_obj = config->phaseN_conductor;
		}

		//See if Phase exists
		if (temp_obj != NULL)
		{
			//Get continuous - summer
			temp_rating_value = get_double(temp_obj,"rating.summer.continuous");

			//Check if NULL
			if (temp_rating_value != NULL)
			{
				//Update - if necessary
				if (temp_rating_continuous > *temp_rating_value)
				{
					temp_rating_continuous = *temp_rating_value;
				}
			}

			//Get continuous - winter
			temp_rating_value = get_double(temp_obj,"rating.winter.continuous");

			//Check if NULL
			if (temp_rating_value != NULL)
			{
				//Update - if necessary
				if (temp_rating_continuous > *temp_rating_value)
				{
					temp_rating_continuous = *temp_rating_value;
				}
			}

			//Now get emergency - summer
			temp_rating_value = get_double(temp_obj,"rating.summer.emergency");

			//Check if NULL
			if (temp_rating_value != NULL)
			{
				//Update - if necessary
				if (temp_rating_emergency > *temp_rating_value)
				{
					temp_rating_emergency = *temp_rating_value;
				}
			}

			//Now get emergency - winter
			temp_rating_value = get_double(temp_obj,"rating.winter.emergency");

			//Check if NULL
			if (temp_rating_value != NULL)
			{
				//Update - if necessary
				if (temp_rating_emergency > *temp_rating_value)
				{
					temp_rating_emergency = *temp_rating_value;
				}
			}
		}//End Phase valid
	}//End FOR

	//Populate link array
	link_rating[0] = temp_rating_continuous;
	link_rating[1] = temp_rating_emergency;

	return result;
}

void underground_line::recalc(void)
{
	line_configuration *config = OBJECTDATA(configuration, line_configuration);
	double miles = length/5280;

	if (config->impedance11 != 0 || config->impedance22 != 0 || config->impedance33 != 0)
	{
		for (int i = 0; i < 3; i++) 
		{
			for (int j = 0; j < 3; j++) 
			{
				b_mat[i][j] = 0.0;
			}
		}
		// User defined z-matrix - only assign parts of matrix if phase exists
		if (has_phase(PHASE_A))
		{
			b_mat[0][0] = config->impedance11 * miles;
			
			if (has_phase(PHASE_B))
			{
				b_mat[0][1] = config->impedance12 * miles;
				b_mat[1][0] = config->impedance21 * miles;
			}
			if (has_phase(PHASE_C))
			{
				b_mat[0][2] = config->impedance13 * miles;
				b_mat[2][0] = config->impedance31 * miles;
			}
		}
		if (has_phase(PHASE_B))
		{
			b_mat[1][1] = config->impedance22 * miles;
			
			if (has_phase(PHASE_C))
			{
				b_mat[1][2] = config->impedance23 * miles;
				b_mat[2][1] = config->impedance32 * miles;
			}
		
		}
		if (has_phase(PHASE_C))
			b_mat[2][2] = config->impedance33 * miles;

		// Set auxillary matrices - make phase dependent
		//Zero/post appropriate values
		for (int i = 0; i < 3; i++) 
		{
			for (int j = 0; j < 3; j++) 
			{
				a_mat[i][j] = d_mat[i][j] = A_mat[i][j] = 0.0;
				c_mat[i][j] = 0.0;
				B_mat[i][j] = b_mat[i][j];
			}
		}

		//"Unzero" appropriate values - doing it this way makes FBS more cooperative
		if (has_phase(PHASE_A))
		{
			a_mat[0][0] = d_mat[0][0] = A_mat[0][0] = 1.0;
		}

		if (has_phase(PHASE_B))
		{
			a_mat[1][1] = d_mat[1][1] = A_mat[1][1] = 1.0;
		}

		if (has_phase(PHASE_C))
		{
			a_mat[2][2] = d_mat[2][2] = A_mat[2][2] = 1.0;
		}
	}
	else
	{
		double dia_od1, dia_od2, dia_od3;
		int16 strands_4, strands_5, strands_6;
		double rad_14, rad_25, rad_36;
		double dia[7], res[7], gmr[7], gmrcn[3], rcn[3];
		double d[6][6];
		double perm_A, perm_B, perm_C, c_an, c_bn, c_cn, temp_denom;
		complex cap_freq_coeff;
		complex z[6][6]; //, z_ij[3][3], z_in[3][3], z_nj[3][3], z_nn[3][3], z_abc[3][3];
		complex Y_ABC[3][3], U_ABC[3][3], temp_mat[3][3];
		double freq_coeff_real, freq_coeff_imag, freq_additive_term;

		complex test;///////////////

		//Calculate coefficients for self and mutual impedance - incorporates frequency values
		//Per Kersting (4.39) and (4.40) - coefficients end up same as OHLs
		freq_coeff_real = 0.00158836*nominal_frequency;
		freq_coeff_imag = 0.00202237*nominal_frequency;
		freq_additive_term = log(EARTH_RESISTIVITY/nominal_frequency)/2.0 + 7.6786;

		#define DIA(i) (dia[i - 1])
		#define RES(i) (res[i - 1])
		#define GMR(i) (gmr[i - 1])
		#define GMRCN(i) (gmrcn[i - 4])
		#define RCN(i) (rcn[i - 4])
		#define D(i, j) (d[i - 1][j - 1])
		#define Z(i, j) (z[i - 1][j - 1])

		#define UG_GET(ph, name) (has_phase(PHASE_##ph) && config->phase##ph##_conductor ? \
				OBJECTDATA(config->phase##ph##_conductor, underground_line_conductor)->name : 0)

		dia_od1 = UG_GET(A, outer_diameter);
		dia_od2 = UG_GET(B, outer_diameter);
		dia_od3 = UG_GET(C, outer_diameter);
		GMR(1) = UG_GET(A, conductor_gmr);
		GMR(2) = UG_GET(B, conductor_gmr);
		GMR(3) = UG_GET(C, conductor_gmr);
		GMR(7) = UG_GET(N, conductor_gmr);
		DIA(1) = UG_GET(A, conductor_diameter);
		DIA(2) = UG_GET(B, conductor_diameter);
		DIA(3) = UG_GET(C, conductor_diameter);
		DIA(7) = UG_GET(N, conductor_diameter);
		RES(1) = UG_GET(A, conductor_resistance);
		RES(2) = UG_GET(B, conductor_resistance);
		RES(3) = UG_GET(C, conductor_resistance);
		RES(7) = UG_GET(N, conductor_resistance);
		GMR(4) = UG_GET(A, neutral_gmr);
		GMR(5) = UG_GET(B, neutral_gmr);
		GMR(6) = UG_GET(C, neutral_gmr);
		DIA(4) = UG_GET(A, neutral_diameter);
		DIA(5) = UG_GET(B, neutral_diameter);
		DIA(6) = UG_GET(C, neutral_diameter);
		RES(4) = UG_GET(A, neutral_resistance);
		RES(5) = UG_GET(B, neutral_resistance);
		RES(6) = UG_GET(C, neutral_resistance);
		strands_4 = UG_GET(A, neutral_strands);
		strands_5 = UG_GET(B, neutral_strands);
		strands_6 = UG_GET(C, neutral_strands);
		rad_14 = (dia_od1 - DIA(4)) / 24.0;
		rad_25 = (dia_od2 - DIA(5)) / 24.0;
		rad_36 = (dia_od3 - DIA(6)) / 24.0;

		RCN(4) = has_phase(PHASE_A) && strands_4 > 0 ? RES(4) / strands_4 : 0.0;
		RCN(5) = has_phase(PHASE_B) && strands_5 > 0 ? RES(5) / strands_5 : 0.0;
		RCN(6) = has_phase(PHASE_C) && strands_6 > 0 ? RES(6) / strands_6 : 0.0;

		//Concentric neutral code
		GMRCN(4) = !(has_phase(PHASE_A) && strands_4 > 0) ? 0.0 :
			pow(GMR(4) * strands_4 * pow(rad_14, (strands_4 - 1)), (1.0 / strands_4));
		GMRCN(5) = !(has_phase(PHASE_B) && strands_5 > 0) ? 0.0 :
			pow(GMR(5) * strands_5 * pow(rad_25, (strands_5 - 1)), (1.0 / strands_5));
		GMRCN(6) = !(has_phase(PHASE_C) && strands_6 > 0) ? 0.0 :
			pow(GMR(6) * strands_6 * pow(rad_36, (strands_6 - 1)), (1.0 / strands_6));

		//Capacitance stuff, if desired
		if (use_line_cap == true)
		{
			//Extract relative permitivitty
			perm_A = UG_GET(A, insulation_rel_permitivitty);
			perm_B = UG_GET(B, insulation_rel_permitivitty);
			perm_C = UG_GET(C, insulation_rel_permitivitty);

			//Define the scaling constant for frequency, distance, and microS
			cap_freq_coeff = complex(0,(2.0*PI*nominal_frequency*0.000001*miles));

			//Zero capacitance matrix
			Y_ABC[0][0] = Y_ABC[0][1] = Y_ABC[0][2] = 0.0;
			Y_ABC[1][0] = Y_ABC[1][1] = Y_ABC[1][2] = 0.0;
			Y_ABC[2][0] = Y_ABC[2][1] = Y_ABC[2][2] = 0.0;
		}

		#define DIST(ph1, ph2) (has_phase(PHASE_##ph1) && has_phase(PHASE_##ph2) && config->line_spacing ? \
			OBJECTDATA(config->line_spacing, line_spacing)->distance_##ph1##to##ph2 : 0.0)

		D(1, 2) = DIST(A, B);
		D(1, 3) = DIST(A, C);
		D(1, 4) = rad_14;
		D(1, 5) = D(1, 2);
		D(1, 6) = D(1, 3);
		D(2, 1) = D(1, 2);
		D(2, 3) = DIST(B, C);
		D(2, 4) = D(2, 1);
		D(2, 5) = rad_25;
		D(2, 6) = D(2, 3);
		D(3, 1) = D(1, 3);
		D(3, 2) = D(2, 3);
		D(3, 4) = D(3, 1);
		D(3, 5) = D(3, 2);
		D(3, 6) = rad_36;
		D(4, 1) = D(1, 4);
		D(4, 2) = D(2, 4);
		D(4, 3) = D(3, 4);
		D(4, 5) = D(1, 2);
		D(4, 6) = D(1, 3);
		D(5, 1) = D(1, 5);
		D(5, 2) = D(2, 5);
		D(5, 3) = D(3, 5);
		D(5, 4) = D(4, 5);
		D(5, 6) = D(2, 3);
		D(6, 1) = D(1, 6);
		D(6, 2) = D(2, 6);
		D(6, 3) = D(3, 6);
		D(6, 4) = D(4, 6);
		D(6, 5) = D(5, 6);

		#undef DIST
		#undef DIA
		#undef UG_GET

		#define Z_GMR(i) (GMR(i) == 0.0 ? complex(0.0) : complex(freq_coeff_real + RES(i), freq_coeff_imag * (log(1.0 / GMR(i)) + freq_additive_term)))
		#define Z_GMRCN(i) (GMRCN(i) == 0.0 ? complex(0.0) : complex(freq_coeff_real + RCN(i), freq_coeff_imag * (log(1.0 / GMRCN(i)) + freq_additive_term)))
		#define Z_DIST(i, j) (D(i, j) == 0.0 ? complex(0.0) : complex(freq_coeff_real, freq_coeff_imag * (log(1.0 / D(i, j)) + freq_additive_term)))

		for (int i = 1; i < 7; i++) {
			for (int j = 1; j < 7; j++) {
				if (i == j) {
					if (i > 3){
						Z(i, j) = Z_GMRCN(i);
						test=Z_GMRCN(i);
					}
					else
						Z(i, j) = Z_GMR(i);
				}
				else
					Z(i, j) = Z_DIST(i, j);
			}
		}	


		#undef RES
		#undef GMR
		#undef GMRCN
		#undef RCN
		#undef D
		#undef Z_GMR
		#undef Z_GMRCN
		#undef Z_DIST

		complex z_ij[3][3] = {{Z(1, 1), Z(1, 2), Z(1, 3)},
							  {Z(2, 1), Z(2, 2), Z(2, 3)},
							  {Z(3, 1), Z(3, 2), Z(3, 3)}};
		complex z_in[3][3] = {{Z(1, 4), Z(1, 5), Z(1, 6)},
							  {Z(2, 4), Z(2, 5), Z(2, 6)},
							  {Z(3, 4), Z(3, 5), Z(3, 6)}};
		complex z_nj[3][3] = {{Z(1, 4), Z(2, 4), Z(3, 4)},
							  {Z(1, 5), Z(2, 5), Z(3, 5)},
							  {Z(1, 6), Z(2, 6), Z(3, 6)}};
		complex z_nn[3][3] = {{Z(4, 4), Z(4, 5), Z(4, 6)},
							  {Z(5, 4), Z(5, 5), Z(5, 6)},
							  {Z(6, 4), Z(6, 5), Z(6, 6)}};
		complex z_nn_inv[3][3], z_p1[3][3], z_p2[3][3], z_abc[3][3];

		#undef Z

		if (!(has_phase(PHASE_A)&&has_phase(PHASE_B)&&has_phase(PHASE_C))){
			if (!has_phase(PHASE_A))
				z_nn[0][0]=complex(1.0);
			if (!has_phase(PHASE_B))
				z_nn[1][1]=complex(1.0);
			if (!has_phase(PHASE_C))
				z_nn[2][2]=complex(1.0);
		}
		
		inverse(z_nn,z_nn_inv);
		multiply(z_in, z_nn_inv, z_p1);
		multiply(z_p1, z_nj, z_p2);
		subtract(z_ij, z_p2, z_abc);
		multiply(miles, z_abc, b_mat);

		if (use_line_cap == true)
		{
			//Compute base capacitances - split denominator to handle 
			if (has_phase(PHASE_A))
			{
				if ((dia[0]==0.0) || (rad_14==0.0) || (strands_4 == 0))	//Make sure conductor or "neutral ring" radius are not zero
				{
					gl_warning("Unable to compute capacitance for %s",OBJECTHDR(this)->name);
					/* TROUBLESHOOT
					One phase of an underground line has either a conductor diameter, a concentric-neutral location diameter, or a neutral
					strand count of zero.  This will lead to indeterminant values in the analysis.  Please fix these values, or run the simulation
					with line capacitance disabled.
					*/
					
					c_an = 0.0;
				}
				else	//All should be OK
				{
					//Compute the denominator (make sure it isn't zero)
					temp_denom = log(rad_14/(dia[0] / 24.0)) - (1.0 / ((double)(strands_4))) * log(((double)(strands_4))*dia[3] / 24.0 / rad_14);

					if (temp_denom == 0.0)
					{
						gl_warning("Capacitance calculation failure for %s",OBJECTHDR(this)->name);
						/*  TROUBLESHOOT
						While computing the capacitance, a zero-value denominator was encountered.  Please check
						your underground_conductor parameter values and try again.
						*/
						
						c_an = 0.0;
					}
					else	//Valid, continue
					{
						//Calculate capacitance value for A
						c_an = (2.0 * PI * PERMITIVITTY_FREE * perm_A) / temp_denom;
					}
				}
			}
			else //No phase A
			{
				c_an = 0.0;	//No capacitance
			}

			//Compute base capacitances - split denominator to handle 
			if (has_phase(PHASE_B))
			{
				if ((dia[1]==0.0) || (rad_25==0.0) || (strands_5 == 0))	//Make sure conductor or "neutral ring" radius are not zero
				{
					gl_warning("Unable to compute capacitance for %s",OBJECTHDR(this)->name);
					//Defined above

					c_bn = 0.0;
				}
				else	//All should be OK
				{
					//Compute the denominator (make sure it isn't zero)
					temp_denom = log(rad_25/(dia[1] / 24.0)) - (1.0 / ((double)(strands_5))) * log(((double)(strands_5))*dia[4] / 24.0 / rad_25);

					if (temp_denom == 0.0)
					{
						gl_warning("Capacitance calculation failure for %s",OBJECTHDR(this)->name);
						//Defined above

						c_bn = 0.0;
					}
					else	//Valid, continue
					{
						//Calculate capacitance value for A
						c_bn = (2.0 * PI * PERMITIVITTY_FREE * perm_B) / temp_denom;
					}
				}
			}
			else //No phase B
			{
				c_bn = 0.0;	//No capacitance
			}

			//Compute base capacitances - split denominator to handle 
			if (has_phase(PHASE_C))
			{
				if ((dia[2]==0.0) || (rad_36==0.0) || (strands_6 == 0))	//Make sure conductor or "neutral ring" radius are not zero
				{
					gl_warning("Unable to compute capacitance for %s",OBJECTHDR(this)->name);
					//Defined above

					c_cn = 0.0;
				}
				else	//All should be OK
				{
					//Compute the denominator (make sure it isn't zero)
					temp_denom = log(rad_36/(dia[2] / 24.0)) - (1.0 / ((double)(strands_6))) * log(((double)(strands_6))*dia[5] / 24.0 / rad_36);

					if (temp_denom == 0.0)
					{
						gl_warning("Capacitance calculation failure for %s",OBJECTHDR(this)->name);
						//Defined above

						c_cn = 0.0;
					}
					else	//Valid, continue
					{
						//Calculate capacitance value for C
						c_cn = (2.0 * PI * PERMITIVITTY_FREE * perm_C) / temp_denom;
					}
				}
			}
			else //No phase C
			{
				c_cn = 0.0;	//No capacitance
			}

			//Make admittance matrix
			Y_ABC[0][0] = cap_freq_coeff * c_an;
			Y_ABC[1][1] = cap_freq_coeff * c_bn;
			Y_ABC[2][2] = cap_freq_coeff * c_cn;

			//Y_ABC = Cabc_mat

			//Create identity matrix
			U_ABC[0][0] = U_ABC[1][1] = U_ABC[2][2] = 1.0;
			U_ABC[0][1] = U_ABC[0][2] = 0.0;
			U_ABC[1][0] = U_ABC[1][2] = 0.0;
			U_ABC[2][0] = U_ABC[2][1] = 0.0;

			//a_mat & d_mat
				//Zabc*Yabc
				multiply(b_mat,Y_ABC,temp_mat);

				//0.5*Above - use A_mat temporarily
				multiply(0.5,temp_mat,A_mat);

				//Add unity to make a_mat
				addition(U_ABC,A_mat,a_mat);

				//d_mat is the same as a_mat
				equalm(a_mat,d_mat);

			//c_mat
				//Zabc*Yabc is temp_mat from above
				//So Yabc*(Zabc*Yabc) - use A_mat again
				multiply(Y_ABC,temp_mat,A_mat);

				//Multiply by 1/4 - use B_mat temporarily
				multiply(0.25,A_mat,B_mat);

				//Add to Yabc
				addition(Y_ABC,B_mat,c_mat);

			//A_mat is phase dependent inversion - B_mat is a product associated with it
			//Zero them first
			for (int i = 0; i < 3; i++) 
			{
				for (int j = 0; j < 3; j++) 
				{
					A_mat[i][j] = B_mat[i][j] = 0.0;
				}
			}

			//Invert appropriately - A_mat = inv(a_mat)
			if (has_phase(PHASE_A) && !has_phase(PHASE_B) && !has_phase(PHASE_C)) //only A
			{
				//Inverted value
				A_mat[0][0] = complex(1.0) / a_mat[0][0];
			}
			else if (!has_phase(PHASE_A) && has_phase(PHASE_B) && !has_phase(PHASE_C)) //only B
			{
				//Inverted value
				A_mat[1][1] = complex(1.0) / a_mat[1][1];
			}
			else if (!has_phase(PHASE_A) && !has_phase(PHASE_B) && has_phase(PHASE_C)) //only C
			{
				//Inverted value
				A_mat[2][2] = complex(1.0) / a_mat[2][2];
			}
			else if (has_phase(PHASE_A) && !has_phase(PHASE_B) && has_phase(PHASE_C)) //has A & C
			{
				complex detvalue = a_mat[0][0]*a_mat[2][2] - a_mat[0][2]*a_mat[2][0];

				//Inverted value
				A_mat[0][0] = a_mat[2][2] / detvalue;
				A_mat[0][2] = a_mat[0][2] * -1.0 / detvalue;
				A_mat[2][0] = a_mat[2][0] * -1.0 / detvalue;
				A_mat[2][2] = a_mat[0][0] / detvalue;
			}
			else if (has_phase(PHASE_A) && has_phase(PHASE_B) && !has_phase(PHASE_C)) //has A & B
			{
				complex detvalue = a_mat[0][0]*a_mat[1][1] - a_mat[0][1]*a_mat[1][0];

				//Inverted value
				A_mat[0][0] = a_mat[1][1] / detvalue;
				A_mat[0][1] = a_mat[0][1] * -1.0 / detvalue;
				A_mat[1][0] = a_mat[1][0] * -1.0 / detvalue;
				A_mat[1][1] = a_mat[0][0] / detvalue;
			}
			else if (!has_phase(PHASE_A) && has_phase(PHASE_B) && has_phase(PHASE_C))	//has B & C
			{
				complex detvalue = a_mat[1][1]*a_mat[2][2] - a_mat[1][2]*a_mat[2][1];

				//Inverted value
				A_mat[1][1] = a_mat[2][2] / detvalue;
				A_mat[1][2] = a_mat[1][2] * -1.0 / detvalue;
				A_mat[2][1] = a_mat[2][1] * -1.0 / detvalue;
				A_mat[2][2] = a_mat[1][1] / detvalue;
			}
			else if ((has_phase(PHASE_A) && has_phase(PHASE_B) && has_phase(PHASE_C)) || (has_phase(PHASE_D))) //has ABC or D (D=ABC)
			{
				complex detvalue = a_mat[0][0]*a_mat[1][1]*a_mat[2][2] - a_mat[0][0]*a_mat[1][2]*a_mat[2][1] - a_mat[0][1]*a_mat[1][0]*a_mat[2][2] + a_mat[0][1]*a_mat[2][0]*a_mat[1][2] + a_mat[1][0]*a_mat[0][2]*a_mat[2][1] - a_mat[0][2]*a_mat[1][1]*a_mat[2][0];

				//Invert it
				A_mat[0][0] = (a_mat[1][1]*a_mat[2][2] - a_mat[1][2]*a_mat[2][1]) / detvalue;
				A_mat[0][1] = (a_mat[0][2]*a_mat[2][1] - a_mat[0][1]*a_mat[2][2]) / detvalue;
				A_mat[0][2] = (a_mat[0][1]*a_mat[1][2] - a_mat[0][2]*a_mat[1][1]) / detvalue;
				A_mat[1][0] = (a_mat[2][0]*a_mat[1][2] - a_mat[1][0]*a_mat[2][2]) / detvalue;
				A_mat[1][1] = (a_mat[0][0]*a_mat[2][2] - a_mat[0][2]*a_mat[2][0]) / detvalue;
				A_mat[1][2] = (a_mat[1][0]*a_mat[0][2] - a_mat[0][0]*a_mat[1][2]) / detvalue;
				A_mat[2][0] = (a_mat[1][0]*a_mat[2][1] - a_mat[1][1]*a_mat[2][0]) / detvalue;
				A_mat[2][1] = (a_mat[0][1]*a_mat[2][0] - a_mat[0][0]*a_mat[2][1]) / detvalue;
				A_mat[2][2] = (a_mat[0][0]*a_mat[1][1] - a_mat[0][1]*a_mat[1][0]) / detvalue;
			}

			//Now make B_mat value
			multiply(A_mat,b_mat,B_mat);
		}
		else	//No line capacitance, carry on as usual
		{
			//Zero/set appropriate values
			for (int i = 0; i < 3; i++) 
			{
				for (int j = 0; j < 3; j++) 
				{
					a_mat[i][j] = d_mat[i][j] = A_mat[i][j] = 0.0;
					c_mat[i][j] = 0.0;
					B_mat[i][j] = b_mat[i][j];
				}
			}

			//"Unzero" appropriate values - doing it this way makes FBS more cooperative
			if (has_phase(PHASE_A))
			{
				a_mat[0][0] = d_mat[0][0] = A_mat[0][0] = 1.0;
			}

			if (has_phase(PHASE_B))
			{
				a_mat[1][1] = d_mat[1][1] = A_mat[1][1] = 1.0;
			}

			if (has_phase(PHASE_C))
			{
				a_mat[2][2] = d_mat[2][2] = A_mat[2][2] = 1.0;
			}
		}
	}

#ifdef _TESTING
	/// @todo use output_test() instead of cout (powerflow, high priority) (ticket #137)
	if (show_matrix_values)
	{
		gl_testmsg("underground_line: %d a matrix");
		print_matrix(a_mat);

		gl_testmsg("underground_line: %d A matrix");
		print_matrix(A_mat);

		gl_testmsg("underground_line: %d b matrix");
		print_matrix(b_mat);

		gl_testmsg("underground_line: %d B matrix");
		print_matrix(B_mat);

		gl_testmsg("underground_line: %d c matrix");
		print_matrix(c_mat);

		gl_testmsg("underground_line: %d d matrix");
		print_matrix(d_mat);
	}
#endif
}

int underground_line::isa(char *classname)
{
	return strcmp(classname,"underground_line")==0 || line::isa(classname);
}

/**
* test_phases is called to ensure all necessary conductors are included in the
* configuration object and are of the proper type.
*
* @param config the line configuration object
* @param ph the phase to check
*/
void underground_line::test_phases(line_configuration *config, const char ph)
{
	bool condCheck, condNotPres;
	OBJECT *obj = GETOBJECT(this);

	if (ph=='A')
	{
		if (config->impedance11 == 0.0)
		{
			condCheck = (config->phaseA_conductor && !gl_object_isa(config->phaseA_conductor, "underground_line_conductor"));
			condNotPres = ((!config->phaseA_conductor) && has_phase(PHASE_A));
		}
		else
		{
			condCheck = false;
			condNotPres = false;
		}
	}
	else if (ph=='B')
	{
		if (config->impedance22 == 0.0)
		{
			condCheck = (config->phaseB_conductor && !gl_object_isa(config->phaseB_conductor, "underground_line_conductor"));
			condNotPres = ((!config->phaseB_conductor) && has_phase(PHASE_B));
		}
		else
		{
			condCheck = false;
			condNotPres = false;
		}
	}
	else if (ph=='C')
	{
		if (config->impedance33 == 0.0)
		{
			condCheck = (config->phaseC_conductor && !gl_object_isa(config->phaseC_conductor, "underground_line_conductor"));
			condNotPres = ((!config->phaseC_conductor) && has_phase(PHASE_C));
		}
		else
		{
			condCheck = false;
			condNotPres = false;
		}
	}
	else if (ph=='N')
	{
		if (config->impedance11 == 0.0 && config->impedance22 == 0.0 && config->impedance33 == 0.0)
		{
			condCheck = (config->phaseN_conductor && !gl_object_isa(config->phaseN_conductor, "underground_line_conductor"));
			condNotPres = ((!config->phaseN_conductor) && has_phase(PHASE_N));
		}
		else
		{
			condCheck = false;
			condNotPres = false;
		}
	}
	//Nothing else down here.  Should never get anything besides ABCN to check

	if (condCheck==true)
		GL_THROW("invalid conductor for phase %c of underground line",ph,obj->name);
		/*	TROUBLESHOOT  The conductor specified for the indicated phase is not necessarily an underground line conductor, it may be an overhead or triplex-line only conductor. */

	if (condNotPres==true)
		GL_THROW("missing conductor for phase %c of underground line",ph,obj->name);
		/*  TROUBLESHOOT
		The object specified as the configuration for the underground line is not a valid
		configuration object.  Please ensure you have a line_configuration object selected.
		*/
}
//////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF CORE LINKAGE: underground_line
//////////////////////////////////////////////////////////////////////////

/**
* REQUIRED: allocate and initialize an object.
*
* @param obj a pointer to a pointer of the last object in the list
* @param parent a pointer to the parent of this object
* @return 1 for a successfully created object, 0 for error
*/


/* This can be added back in after tape has been moved to commit
EXPORT int commit_underground_line(OBJECT *obj)
{
	if ((solver_method==SM_FBS) || (solver_method==SM_NR))
	{
		underground_line *plink = OBJECTDATA(obj,underground_line);
		plink->calculate_power();
	}
	return 1;
}*/
EXPORT int create_underground_line(OBJECT **obj, OBJECT *parent)
{
	try
	{
		*obj = gl_create_object(underground_line::oclass);
		if (*obj!=NULL)
		{
			underground_line *my = OBJECTDATA(*obj,underground_line);
			gl_set_parent(*obj,parent);
			return my->create();
		}	}
	catch (const char *msg)
	{
		gl_error("create_underground_line: %s", msg);
	}
	return 0;
}

EXPORT TIMESTAMP sync_underground_line(OBJECT *obj, TIMESTAMP t0, PASSCONFIG pass)
{
	underground_line *pObj = OBJECTDATA(obj,underground_line);
	try {
		TIMESTAMP t1 = TS_NEVER;
		switch (pass) {
		case PC_PRETOPDOWN:
			return pObj->presync(t0);
		case PC_BOTTOMUP:
			return pObj->sync(t0);
		case PC_POSTTOPDOWN:
			t1 = pObj->postsync(t0);
			obj->clock = t0;
			return t1;
		default:
			throw "invalid pass request";
		}
	} catch (const char *error) {
		gl_error("%s (underground_line:%d): %s", pObj->get_name(), pObj->get_id(), error);
		return TS_INVALID; 
	} catch (...) {
		gl_error("%s (underground_line:%d): %s", pObj->get_name(), pObj->get_id(), "unknown exception");
		return TS_INVALID;
	}
}

EXPORT int init_underground_line(OBJECT *obj)
{
	underground_line *my = OBJECTDATA(obj,underground_line);
	try {
		return my->init(obj->parent);
	}
	catch (const char *msg)
	{
		gl_error("%s (underground_line:%d): %s", my->get_name(), my->get_id(), msg);
		return 0; 
	}
}

EXPORT int isa_underground_line(OBJECT *obj, char *classname)
{
	return OBJECTDATA(obj,underground_line)->isa(classname);
}

EXPORT int recalc_underground_line(OBJECT *obj)
{
	OBJECTDATA(obj,underground_line)->recalc();
	return 1;
}

EXPORT int create_fault_ugline(OBJECT *thisobj, OBJECT **protect_obj, char *fault_type, int *implemented_fault, TIMESTAMP *repair_time, void *Extra_Data)
{
	int retval;

	//Link to ourselves
	underground_line *thisline = OBJECTDATA(thisobj,underground_line);

	//Try to fault up
	retval = thisline->link_fault_on(protect_obj, fault_type, implemented_fault,repair_time,Extra_Data);

	return retval;
}
EXPORT int fix_fault_ugline(OBJECT *thisobj, int *implemented_fault, char *imp_fault_name, void *Extra_Data)
{
	int retval;

	//Link to ourselves
	underground_line *thisline = OBJECTDATA(thisobj,underground_line);

	//Clear the fault
	retval = thisline->link_fault_off(implemented_fault, imp_fault_name, Extra_Data);

	//Clear the fault type
	*implemented_fault = -1;

	return retval;
}
/**@}**/