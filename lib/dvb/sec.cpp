#include <lib/dvb/dvb.h>
#include <lib/dvb/sec.h>
#include <lib/dvb/rotor_calc.h>

#include <set>

#if HAVE_DVB_API_VERSION < 3
#define FREQUENCY Frequency
#else
#define FREQUENCY frequency
#endif
#include <lib/base/eerror.h>

DEFINE_REF(eDVBSatelliteEquipmentControl);

eDVBSatelliteEquipmentControl *eDVBSatelliteEquipmentControl::instance;

eDVBSatelliteEquipmentControl::eDVBSatelliteEquipmentControl(eSmartPtrList<eDVBRegisteredFrontend> &avail_frontends)
	:m_lnbidx(-1), m_curSat(m_lnbs[0].m_satellites.end()), m_avail_frontends(avail_frontends), m_rotorMoving(false)
{
	if (!instance)
		instance = this;

	clear();

// ASTRA
	addLNB();
	setLNBTunerMask(3);
	setLNBLOFL(9750000);
	setLNBThreshold(11750000);
	setLNBLOFH(10607000);
	setDiSEqCMode(eDVBSatelliteDiseqcParameters::V1_0);
	setToneburst(eDVBSatelliteDiseqcParameters::NO);
	setRepeats(0);
	setCommittedCommand(eDVBSatelliteDiseqcParameters::BB);
	setCommandOrder(0); // committed, toneburst
	setFastDiSEqC(true);
	setSeqRepeat(false);
	addSatellite(192);
	setVoltageMode(eDVBSatelliteSwitchParameters::HV);
	setToneMode(eDVBSatelliteSwitchParameters::HILO);

// Hotbird
	addLNB();
	setLNBTunerMask(3);
	setLNBLOFL(9750000);
	setLNBThreshold(11750000);
	setLNBLOFH(10600000);
	setDiSEqCMode(eDVBSatelliteDiseqcParameters::V1_0);
	setToneburst(eDVBSatelliteDiseqcParameters::NO);
	setRepeats(0);
	setCommittedCommand(eDVBSatelliteDiseqcParameters::AB);
	setCommandOrder(0); // committed, toneburst
	setFastDiSEqC(true);
	setSeqRepeat(false);
	addSatellite(130);
	setVoltageMode(eDVBSatelliteSwitchParameters::HV);
	setToneMode(eDVBSatelliteSwitchParameters::HILO);

// Rotor
	addLNB();
	setLNBTunerMask(3);
	setLNBLOFL(9750000);
	setLNBThreshold(11750000);
	setLNBLOFH(10600000);
	setDiSEqCMode(eDVBSatelliteDiseqcParameters::V1_2);
	setToneburst(eDVBSatelliteDiseqcParameters::NO);
	setRepeats(0);
	setCommittedCommand(eDVBSatelliteDiseqcParameters::AA);
	setCommandOrder(0); // committed, toneburst
	setFastDiSEqC(true);
	setSeqRepeat(false);
	setLaDirection(eDVBSatelliteRotorParameters::NORTH);
	setLoDirection(eDVBSatelliteRotorParameters::EAST);
	setLatitude(51.017);
	setLongitude(8.683);
	setUseInputpower(true);
	setInputpowerDelta(50);

	addSatellite(235);
	setVoltageMode(eDVBSatelliteSwitchParameters::HV);
	setToneMode(eDVBSatelliteSwitchParameters::HILO);
	setRotorPosNum(0);

	addSatellite(284);
	setVoltageMode(eDVBSatelliteSwitchParameters::HV);
	setToneMode(eDVBSatelliteSwitchParameters::HILO);
	setRotorPosNum(0);

	addSatellite(420);
	setVoltageMode(eDVBSatelliteSwitchParameters::HV);
	setToneMode(eDVBSatelliteSwitchParameters::HILO);
	setRotorPosNum(1); // stored pos 1
}

int eDVBSatelliteEquipmentControl::canTune(const eDVBFrontendParametersSatellite &sat, iDVBFrontend *fe, int frontend_id )
{
	int ret=0;

	for (int idx=0; idx <= m_lnbidx; ++idx )
	{
		eDVBSatelliteLNBParameters &lnb_param = m_lnbs[idx];
		if ( lnb_param.tuner_mask & frontend_id ) // lnb for correct tuner?
		{
			eDVBSatelliteDiseqcParameters &di_param = lnb_param.m_diseqc_parameters;

			std::map<int, eDVBSatelliteSwitchParameters>::iterator sit =
				lnb_param.m_satellites.find(sat.orbital_position);
			if ( sit != lnb_param.m_satellites.end())
			{
				int band=0,
					linked_to=-1, // linked tuner
					satpos_depends_to=-1,
					csw = di_param.m_committed_cmd,
					ucsw = di_param.m_uncommitted_cmd,
					toneburst = di_param.m_toneburst_param,
					curRotorPos;

				fe->getData(6, curRotorPos);
				fe->getData(7, linked_to);
				fe->getData(8, satpos_depends_to);

				if ( sat.frequency > lnb_param.m_lof_threshold )
					band |= 1;
				if (!(sat.polarisation & eDVBFrontendParametersSatellite::Polarisation::Vertical))
					band |= 2;

				bool rotor=false;
				bool diseqc=false;

				if (di_param.m_diseqc_mode >= eDVBSatelliteDiseqcParameters::V1_0)
				{
					diseqc=true;
					if ( di_param.m_committed_cmd < eDVBSatelliteDiseqcParameters::SENDNO )
						csw = 0xF0 | (csw << 2);

					if (di_param.m_committed_cmd <= eDVBSatelliteDiseqcParameters::SENDNO)
						csw |= band;

					if ( di_param.m_diseqc_mode == eDVBSatelliteDiseqcParameters::V1_2 )  // ROTOR
					{
						rotor=true;
						if ( curRotorPos == sat.orbital_position )
							ret=20;  // rotor on correct orbpos = prio 20
						else
							ret=10;  // rotor must turn to correct orbpos = prio 10
					}
					else
						ret = 30;  // no rotor = prio 30
				}
				else
				{
					csw = band;
					ret = 40;  // no diseqc = prio 40
				}

				if (linked_to != -1)  // check for linked tuners..
				{
					eDVBRegisteredFrontend *linked_fe = (eDVBRegisteredFrontend*) linked_to;
					if (linked_fe->m_inuse)
					{
						int ocsw = -1,
							oucsw = -1,
							oToneburst = -1,
							oRotorPos = -1;
						linked_fe->m_frontend->getData(0, ocsw);
						linked_fe->m_frontend->getData(1, oucsw);
						linked_fe->m_frontend->getData(2, oToneburst);
						linked_fe->m_frontend->getData(6, oRotorPos);
#if 0
						eDebug("compare csw %02x == lcsw %02x",
							csw, ocsw);
						if ( diseqc )
							eDebug("compare ucsw %02x == lucsw %02x\ncompare toneburst %02x == oToneburst %02x",
								ucsw, oucsw, toneburst, oToneburst);
						if ( rotor )
							eDebug("compare pos %d == current pos %d",
								sat.orbital_position, oRotorPos);
#endif
						if ( (csw != ocsw) ||
							( diseqc && (ucsw != oucsw || toneburst != oToneburst) ) ||
							( rotor && oRotorPos != sat.orbital_position ) )
						{
//							eDebug("can not tune this transponder with linked tuner in use!!");
							ret=0;
						}
//						else
//							eDebug("OK .. can tune this transponder with linked tuner in use :)");
					}
				}

				if (satpos_depends_to != -1)  // check for linked tuners..
				{
					eDVBRegisteredFrontend *satpos_depends_to_fe = (eDVBRegisteredFrontend*) satpos_depends_to;
					if ( satpos_depends_to_fe->m_inuse )
					{
						int oRotorPos = -1;
						satpos_depends_to_fe->m_frontend->getData(6, oRotorPos);
						if (!rotor || oRotorPos != sat.orbital_position)
						{
//							eDebug("can not tune this transponder ... rotor on other tuner is positioned to %d", oRotorPos);
							ret=0;
						}
					}
//					else
//						eDebug("OK .. can tune this transponder satpos is correct :)");
				}
				if (ret)
				{
					int lof = sat.frequency > lnb_param.m_lof_threshold ?
						lnb_param.m_lof_hi : lnb_param.m_lof_lo;
					int tuner_freq = abs(sat.frequency - lof);
//					eDebug("tuner freq %d", tuner_freq);
					if (tuner_freq < 900000 || tuner_freq > 2200000)
					{
						ret=0;
//						eDebug("Transponder not tuneable with this lnb... %d Khz out of tuner range",
//							tuner_freq);
					}
				}
			}
		}
	}
	return ret;
}

#define VOLTAGE(x) (lnb_param.m_increased_voltage ? iDVBFrontend::voltage##x##_5 : iDVBFrontend::voltage##x)

RESULT eDVBSatelliteEquipmentControl::prepare(iDVBFrontend &frontend, FRONTENDPARAMETERS &parm, const eDVBFrontendParametersSatellite &sat, int frontend_id)
{
	bool linked=false;
	bool depend_satpos_mode=false;

	for (int idx=0; idx <= m_lnbidx; ++idx )
	{
		eDVBSatelliteLNBParameters &lnb_param = m_lnbs[idx];
		if (!(lnb_param.tuner_mask & frontend_id)) // lnb for correct tuner?
			continue;
		eDVBSatelliteDiseqcParameters &di_param = lnb_param.m_diseqc_parameters;
		eDVBSatelliteRotorParameters &rotor_param = lnb_param.m_rotor_parameters;

		std::map<int, eDVBSatelliteSwitchParameters>::iterator sit =
			lnb_param.m_satellites.find(sat.orbital_position);
		if ( sit != lnb_param.m_satellites.end())
		{
			eDVBSatelliteSwitchParameters &sw_param = sit->second;
			bool doSetVoltageToneFrontend = true;
			bool doSetFrontend = true;
			int band=0,
				linked_to=-1, // linked tuner
				satpos_depends_to=-1,
				voltage = iDVBFrontend::voltageOff,
				tone = iDVBFrontend::toneOff,
				csw = di_param.m_committed_cmd,
				ucsw = di_param.m_uncommitted_cmd,
				toneburst = di_param.m_toneburst_param,
				lastcsw = -1,
				lastucsw = -1,
				lastToneburst = -1,
				lastRotorCmd = -1,
				curRotorPos = -1;

			frontend.getData(0, lastcsw);
			frontend.getData(1, lastucsw);
			frontend.getData(2, lastToneburst);
			frontend.getData(5, lastRotorCmd);
			frontend.getData(6, curRotorPos);
			frontend.getData(7, linked_to);
			frontend.getData(8, satpos_depends_to);

			if (linked_to != -1)
			{
				eDVBRegisteredFrontend *linked_fe = (eDVBRegisteredFrontend*) linked_to;
				if (linked_fe->m_inuse)
				{
					eDebug("[SEC] frontend is linked with another and the other one is in use.. so we dont do SEC!!");
					linked=true;
				}
			}

			if (satpos_depends_to != -1)
			{
				eDVBRegisteredFrontend *satpos_fe = (eDVBRegisteredFrontend*) satpos_depends_to;
				if (satpos_fe->m_inuse)
				{
					if ( di_param.m_diseqc_mode != eDVBSatelliteDiseqcParameters::V1_2 )
						continue;
					eDebug("[SEC] frontend is depending on satpos of other one.. so we dont turn rotor!!");
					depend_satpos_mode=true;
				}
			}

			if ( sat.frequency > lnb_param.m_lof_threshold )
				band |= 1;

			if (band&1)
				parm.FREQUENCY = sat.frequency - lnb_param.m_lof_hi;
			else
				parm.FREQUENCY = sat.frequency - lnb_param.m_lof_lo;

			parm.FREQUENCY = abs(parm.FREQUENCY);

			frontend.setData(9, sat.frequency - parm.FREQUENCY);

			if (!(sat.polarisation & eDVBFrontendParametersSatellite::Polarisation::Vertical))
				band |= 2;

			if ( sw_param.m_voltage_mode == eDVBSatelliteSwitchParameters::_14V
				|| ( sat.polarisation & eDVBFrontendParametersSatellite::Polarisation::Vertical
					&& sw_param.m_voltage_mode == eDVBSatelliteSwitchParameters::HV )  )
				voltage = VOLTAGE(13);
			else if ( sw_param.m_voltage_mode == eDVBSatelliteSwitchParameters::_18V
				|| ( !(sat.polarisation & eDVBFrontendParametersSatellite::Polarisation::Vertical)
					&& sw_param.m_voltage_mode == eDVBSatelliteSwitchParameters::HV )  )
				voltage = VOLTAGE(18);
			if ( (sw_param.m_22khz_signal == eDVBSatelliteSwitchParameters::ON)
				|| ( sw_param.m_22khz_signal == eDVBSatelliteSwitchParameters::HILO && (band&1) ) )
				tone = iDVBFrontend::toneOn;
			else if ( (sw_param.m_22khz_signal == eDVBSatelliteSwitchParameters::OFF)
				|| ( sw_param.m_22khz_signal == eDVBSatelliteSwitchParameters::HILO && !(band&1) ) )
				tone = iDVBFrontend::toneOff;

			eSecCommandList sec_sequence;

			if (di_param.m_diseqc_mode >= eDVBSatelliteDiseqcParameters::V1_0)
			{
				if ( di_param.m_committed_cmd < eDVBSatelliteDiseqcParameters::SENDNO )
					csw = 0xF0 | (csw << 2);

				if (di_param.m_committed_cmd <= eDVBSatelliteDiseqcParameters::SENDNO)
					csw |= band;

				bool send_csw =
					(di_param.m_committed_cmd != eDVBSatelliteDiseqcParameters::SENDNO);
				bool changed_csw = send_csw && csw != lastcsw;

				bool send_ucsw =
					(di_param.m_uncommitted_cmd && di_param.m_diseqc_mode > eDVBSatelliteDiseqcParameters::V1_0);
				bool changed_ucsw = send_ucsw && ucsw != lastucsw;

				bool send_burst =
					(di_param.m_toneburst_param != eDVBSatelliteDiseqcParameters::NO);
				bool changed_burst = send_burst && toneburst != lastToneburst;

				int send_mask = 0; /*
					1 must send csw
					2 must send ucsw
					4 send toneburst first
					8 send toneburst at end */
				if (changed_burst) // toneburst first and toneburst changed
				{
					if (di_param.m_command_order&1)
					{
						send_mask |= 4;
						if ( send_csw )
							send_mask |= 1;
						if ( send_ucsw )
							send_mask |= 2;
					}
					else
						send_mask |= 8;
				}
				if (changed_ucsw)
				{
					send_mask |= 2;
					if ((di_param.m_command_order&4) && send_csw)
						send_mask |= 1;
					if (di_param.m_command_order==4 && send_burst)
						send_mask |= 8;
				}
				if (changed_csw) 
				{
					if ( di_param.m_use_fast
						&& di_param.m_committed_cmd < eDVBSatelliteDiseqcParameters::SENDNO
						&& (lastcsw & 0xF0)
						&& ((csw / 4) == (lastcsw / 4)) )
						eDebug("dont send committed cmd (fast diseqc)");
					else
					{
						send_mask |= 1;
						if (!(di_param.m_command_order&4) && send_ucsw)
							send_mask |= 2;
						if (!(di_param.m_command_order&1) && send_burst)
							send_mask |= 8;
					}
				}

#if 0
				eDebugNoNewLine("sendmask: ");
				for (int i=3; i >= 0; --i)
					if ( send_mask & (1<<i) )
						eDebugNoNewLine("1");
					else
						eDebugNoNewLine("0");
				eDebug("");
#endif

				int RotorCmd=-1;
				bool useGotoXX = false;
				if ( di_param.m_diseqc_mode == eDVBSatelliteDiseqcParameters::V1_2
					&& !sat.no_rotor_command_on_tune )
				{
					if (depend_satpos_mode || linked)
						// in this both modes we dont really turn the rotor.... but in canTune we need the satpos
						frontend.setData(6, sat.orbital_position);
					else
					{
						if (sw_param.m_rotorPosNum) // we have stored rotor pos?
							RotorCmd=sw_param.m_rotorPosNum;
						else  // we must calc gotoxx cmd
						{
							eDebug("Entry for %d,%d? not in Rotor Table found... i try gotoXX?", sat.orbital_position / 10, sat.orbital_position % 10 );
							useGotoXX = true;

							double	SatLon = abs(sat.orbital_position)/10.00,
									SiteLat = rotor_param.m_gotoxx_parameters.m_latitude,
									SiteLon = rotor_param.m_gotoxx_parameters.m_longitude;

							if ( rotor_param.m_gotoxx_parameters.m_la_direction == eDVBSatelliteRotorParameters::SOUTH )
								SiteLat = -SiteLat;

							if ( rotor_param.m_gotoxx_parameters.m_lo_direction == eDVBSatelliteRotorParameters::WEST )
								SiteLon = 360 - SiteLon;

							eDebug("siteLatitude = %lf, siteLongitude = %lf, %lf degrees", SiteLat, SiteLon, SatLon );
							double satHourAngle =
								calcSatHourangle( SatLon, SiteLat, SiteLon );
							eDebug("PolarmountHourAngle=%lf", satHourAngle );

							static int gotoXTable[10] =
								{ 0x00, 0x02, 0x03, 0x05, 0x06, 0x08, 0x0A, 0x0B, 0x0D, 0x0E };

							if (SiteLat >= 0) // Northern Hemisphere
							{
								int tmp=(int)round( fabs( 180 - satHourAngle ) * 10.0 );
								RotorCmd = (tmp/10)*0x10 + gotoXTable[ tmp % 10 ];

								if (satHourAngle < 180) // the east
									RotorCmd |= 0xE000;
								else					// west
									RotorCmd |= 0xD000;
							}
							else // Southern Hemisphere
							{
								if (satHourAngle < 180) // the east
								{
									int tmp=(int)round( fabs( satHourAngle ) * 10.0 );
									RotorCmd = (tmp/10)*0x10 + gotoXTable[ tmp % 10 ];
									RotorCmd |= 0xD000;
								}
								else					// west
								{
									int tmp=(int)round( fabs( 360 - satHourAngle ) * 10.0 );
									RotorCmd = (tmp/10)*0x10 + gotoXTable[ tmp % 10 ];
									RotorCmd |= 0xE000;
								}
							}
							eDebug("RotorCmd = %04x", RotorCmd);
						}
					}
				}

				if ( send_mask )
				{
					sec_sequence.push_back( eSecCommand(eSecCommand::SET_TONE, iDVBFrontend::toneOff) );
					sec_sequence.push_back( eSecCommand(eSecCommand::SLEEP, 15) );
					eSecCommand::pair compare;
					compare.voltage = iDVBFrontend::voltageOff;
					compare.steps = +4;
					// the next is a check if voltage is switched off.. then we first set a voltage :)
					// else we set voltage after all diseqc stuff..
					sec_sequence.push_back( eSecCommand(eSecCommand::IF_NOT_VOLTAGE_GOTO, compare) );

					if ( RotorCmd != -1 && RotorCmd != lastRotorCmd )
					{
						if (rotor_param.m_inputpower_parameters.m_use)
							compare.voltage = VOLTAGE(18);  // in input power mode turn rotor always with 18V (fast)
						else
							compare.voltage = VOLTAGE(13);  // in normal mode start turning with 13V
					}
					else
						compare.voltage = voltage;

					// voltage already correct ?
					sec_sequence.push_back( eSecCommand(eSecCommand::IF_VOLTAGE_GOTO, compare) );
					compare.steps = +3;
					sec_sequence.push_back( eSecCommand(eSecCommand::SET_VOLTAGE, compare.voltage) );
					// voltage was disabled..so we wait a longer time .. for normal switches 200ms should be enough
					sec_sequence.push_back( eSecCommand(eSecCommand::SLEEP, 200) );

					for (int seq_repeat = 0; seq_repeat < (di_param.m_seq_repeat?2:1); ++seq_repeat)
					{
						if ( send_mask & 4 )
						{
							sec_sequence.push_back( eSecCommand(eSecCommand::SEND_TONEBURST, di_param.m_toneburst_param) );
							sec_sequence.push_back( eSecCommand(eSecCommand::SLEEP, 50) );
						}

						int loops=0;

						if ( send_mask & 1 )
							++loops;
						if ( send_mask & 2 )
							++loops;

						for ( int i=0; i < di_param.m_repeats; ++i )
							loops *= 2;

						for ( int i = 0; i < loops;)  // fill commands...
						{
							eDVBDiseqcCommand diseqc;
							diseqc.len = 4;
							diseqc.data[0] = i ? 0xE1 : 0xE0;
							diseqc.data[1] = 0x10;
							if ( (send_mask & 2) && (di_param.m_command_order & 4) )
							{
								diseqc.data[2] = 0x39;
								diseqc.data[3] = ucsw;
							}
							else if ( send_mask & 1 )
							{
								diseqc.data[2] = 0x38;
								diseqc.data[3] = csw;
							}
							else  // no committed command confed.. so send uncommitted..
							{
								diseqc.data[2] = 0x39;
								diseqc.data[3] = ucsw;
							}
							sec_sequence.push_back( eSecCommand(eSecCommand::SEND_DISEQC, diseqc) );

							i++;
							if ( i < loops )
							{
								int cmd=0;
								if (diseqc.data[2] == 0x38 && (send_mask & 2))
									cmd=0x39;
								else if (diseqc.data[2] == 0x39 && (send_mask & 1))
									cmd=0x38;
								if (cmd)
								{
									static int delay = (120 - 54) / 2;  // standard says 100msek between two repeated commands
									sec_sequence.push_back( eSecCommand(eSecCommand::SLEEP, delay) );
									diseqc.data[2]=cmd;
									diseqc.data[3]=(cmd==0x38) ? csw : ucsw;
									sec_sequence.push_back( eSecCommand(eSecCommand::SEND_DISEQC, diseqc) );
									++i;
									if ( i < loops )
										sec_sequence.push_back( eSecCommand(eSecCommand::SLEEP, delay ) );
									else
										sec_sequence.push_back( eSecCommand(eSecCommand::SLEEP, 50) );
								}
								else  // delay 120msek when no command is in repeat gap
									sec_sequence.push_back( eSecCommand(eSecCommand::SLEEP, 120) );
							}
							else
								sec_sequence.push_back( eSecCommand(eSecCommand::SLEEP, 50) );
						}

						if ( send_mask & 8 )  // toneburst at end of sequence
						{
							sec_sequence.push_back( eSecCommand(eSecCommand::SEND_TONEBURST, di_param.m_toneburst_param) );
							sec_sequence.push_back( eSecCommand(eSecCommand::SLEEP, 50) );
						}
					}
				}

				if ( RotorCmd != -1 && RotorCmd != lastRotorCmd )
				{
					eSecCommand::pair compare;
					compare.voltage = iDVBFrontend::voltageOff;
					compare.steps = +4;
					// the next is a check if voltage is switched off.. then we first set a voltage :)
					// else we set voltage after all diseqc stuff..
					sec_sequence.push_back( eSecCommand(eSecCommand::IF_NOT_VOLTAGE_GOTO, compare) );

					if (rotor_param.m_inputpower_parameters.m_use)
						sec_sequence.push_back( eSecCommand(eSecCommand::SET_VOLTAGE, VOLTAGE(13)) ); // in normal mode start turning with 13V
					else
						sec_sequence.push_back( eSecCommand(eSecCommand::SET_VOLTAGE, VOLTAGE(18)) ); // turn always with 18V

					// voltage was disabled..so we wait a longer time ..
					sec_sequence.push_back( eSecCommand(eSecCommand::SLEEP, 500) );
					sec_sequence.push_back( eSecCommand(eSecCommand::GOTO, +7) );  // no need to send stop rotor cmd

					if (send_mask)
						sec_sequence.push_back( eSecCommand(eSecCommand::SLEEP, 750) ); // wait 750ms after send switch cmd
					else
						sec_sequence.push_back( eSecCommand(eSecCommand::GOTO, +1) );

					eDVBDiseqcCommand diseqc;
					diseqc.len = 3;
					diseqc.data[0] = 0xE0;
					diseqc.data[1] = 0x31;	// positioner
					diseqc.data[2] = 0x60;	// stop
					sec_sequence.push_back( eSecCommand(eSecCommand::IF_ROTORPOS_VALID_GOTO, +5) );
					sec_sequence.push_back( eSecCommand(eSecCommand::SEND_DISEQC, diseqc) );
					sec_sequence.push_back( eSecCommand(eSecCommand::SLEEP, 50) );
					sec_sequence.push_back( eSecCommand(eSecCommand::SEND_DISEQC, diseqc) );
					// wait 300msec after send rotor stop cmd
					sec_sequence.push_back( eSecCommand(eSecCommand::SLEEP, 300) );

					diseqc.data[0] = 0xE0;
					diseqc.data[1] = 0x31;		// positioner
					if ( useGotoXX )
					{
						diseqc.len = 5;
						diseqc.data[2] = 0x6E;	// drive to angular position
						diseqc.data[3] = ((RotorCmd & 0xFF00) / 0x100);
						diseqc.data[4] = RotorCmd & 0xFF;
					}
					else
					{
						diseqc.len = 4;
						diseqc.data[2] = 0x6B;	// goto stored sat position
						diseqc.data[3] = RotorCmd;
						diseqc.data[4] = 0x00;
					}

					if ( rotor_param.m_inputpower_parameters.m_use )
					{ // use measure rotor input power to detect rotor state
						eSecCommand::rotor cmd;
						eSecCommand::pair compare;
						compare.voltage = VOLTAGE(18);
						compare.steps = +2;
						sec_sequence.push_back( eSecCommand(eSecCommand::IF_VOLTAGE_GOTO, compare) );
						sec_sequence.push_back( eSecCommand(eSecCommand::SET_VOLTAGE, compare.voltage) );
// measure idle power values
						sec_sequence.push_back( eSecCommand(eSecCommand::SLEEP, 200) );  // wait 200msec after voltage change
						sec_sequence.push_back( eSecCommand(eSecCommand::MEASURE_IDLE_INPUTPOWER, 1) );
						compare.voltage = 1;
						compare.steps = -2;
						sec_sequence.push_back( eSecCommand(eSecCommand::IF_MEASURE_IDLE_WAS_NOT_OK_GOTO, compare) );
						sec_sequence.push_back( eSecCommand(eSecCommand::SET_VOLTAGE, VOLTAGE(13)) );
						sec_sequence.push_back( eSecCommand(eSecCommand::SLEEP, 200) );  // wait 200msec before measure
						sec_sequence.push_back( eSecCommand(eSecCommand::MEASURE_IDLE_INPUTPOWER, 0) );
						compare.voltage = 0;
						sec_sequence.push_back( eSecCommand(eSecCommand::IF_MEASURE_IDLE_WAS_NOT_OK_GOTO, compare) );
////////////////////////////
						sec_sequence.push_back( eSecCommand(eSecCommand::SET_POWER_LIMITING_MODE, eSecCommand::modeStatic) );
						sec_sequence.push_back( eSecCommand(eSecCommand::SET_ROTOR_DISEQC_RETRYS, 2) );  // 2 retries
						sec_sequence.push_back( eSecCommand(eSecCommand::INVALIDATE_CURRENT_ROTORPARMS) );
						sec_sequence.push_back( eSecCommand(eSecCommand::SEND_DISEQC, diseqc) );
						sec_sequence.push_back( eSecCommand(eSecCommand::SET_TIMEOUT, 40) );  // 2 seconds rotor start timout
// rotor start loop
						sec_sequence.push_back( eSecCommand(eSecCommand::SLEEP, 50) );  // 50msec delay
						sec_sequence.push_back( eSecCommand(eSecCommand::MEASURE_RUNNING_INPUTPOWER) );
						cmd.direction=1;  // check for running rotor
						cmd.deltaA=rotor_param.m_inputpower_parameters.m_delta;
						cmd.steps=+5;
						cmd.okcount=0;
						sec_sequence.push_back( eSecCommand(eSecCommand::IF_INPUTPOWER_DELTA_GOTO, cmd ) );  // check if rotor has started
						sec_sequence.push_back( eSecCommand(eSecCommand::IF_TIMEOUT_GOTO, +2 ) );  // timeout .. we assume now the rotor is already at the correct position
						sec_sequence.push_back( eSecCommand(eSecCommand::GOTO, -4) );  // goto loop start
						sec_sequence.push_back( eSecCommand(eSecCommand::IF_NO_MORE_ROTOR_DISEQC_RETRYS_GOTO, +9 ) );  // timeout .. we assume now the rotor is already at the correct position
						sec_sequence.push_back( eSecCommand(eSecCommand::GOTO, -8) );  // goto loop start
////////////////////
						sec_sequence.push_back( eSecCommand(eSecCommand::SET_TIMEOUT, 2400) );  // 2 minutes running timeout
						sec_sequence.push_back( eSecCommand(eSecCommand::SET_VOLTAGE, VOLTAGE(18)) );
// rotor running loop
						sec_sequence.push_back( eSecCommand(eSecCommand::SLEEP, 50) );  // wait 50msec
						sec_sequence.push_back( eSecCommand(eSecCommand::MEASURE_RUNNING_INPUTPOWER) );
						cmd.direction=0;  // check for stopped rotor
						cmd.steps=+3;
						sec_sequence.push_back( eSecCommand(eSecCommand::IF_INPUTPOWER_DELTA_GOTO, cmd ) );
						sec_sequence.push_back( eSecCommand(eSecCommand::IF_TIMEOUT_GOTO, +3 ) );  // timeout ? this should never happen
						sec_sequence.push_back( eSecCommand(eSecCommand::GOTO, -4) );  // running loop start
/////////////////////
					}
					else
					{  // use normal turning mode
						doSetVoltageToneFrontend=false;
						doSetFrontend=false;
						eSecCommand::rotor cmd;
						eSecCommand::pair compare;
						compare.voltage = VOLTAGE(13);
						compare.steps = +2;
						sec_sequence.push_back( eSecCommand(eSecCommand::IF_VOLTAGE_GOTO, compare) );
						sec_sequence.push_back( eSecCommand(eSecCommand::SET_VOLTAGE, compare.voltage) );
						sec_sequence.push_back( eSecCommand(eSecCommand::SLEEP, 200) );  // wait 200msec after voltage change

						sec_sequence.push_back( eSecCommand(eSecCommand::SET_POWER_LIMITING_MODE, eSecCommand::modeStatic) );
						sec_sequence.push_back( eSecCommand(eSecCommand::INVALIDATE_CURRENT_ROTORPARMS) );
						sec_sequence.push_back( eSecCommand(eSecCommand::SEND_DISEQC, diseqc) );

						compare.voltage = voltage;
						compare.steps = +3;
						sec_sequence.push_back( eSecCommand(eSecCommand::IF_VOLTAGE_GOTO, compare) ); // correct final voltage?
						sec_sequence.push_back( eSecCommand(eSecCommand::SLEEP, 2000) );  // wait 2 second before set high voltage
						sec_sequence.push_back( eSecCommand(eSecCommand::SET_VOLTAGE, voltage) );

						sec_sequence.push_back( eSecCommand(eSecCommand::SET_TONE, tone) );
						sec_sequence.push_back( eSecCommand(eSecCommand::SET_FRONTEND) );

						cmd.direction=1;  // check for running rotor
						cmd.deltaA=0;
						cmd.steps=+3;
						cmd.okcount=0;

						sec_sequence.push_back( eSecCommand(eSecCommand::SET_TIMEOUT, 480) );  // 2 minutes running timeout
						sec_sequence.push_back( eSecCommand(eSecCommand::SLEEP, 250) );  // 250msec delay
						sec_sequence.push_back( eSecCommand(eSecCommand::IF_TUNER_LOCKED_GOTO, cmd ) );
						sec_sequence.push_back( eSecCommand(eSecCommand::IF_TIMEOUT_GOTO, +3 ) ); 
						sec_sequence.push_back( eSecCommand(eSecCommand::GOTO, -3) );  // goto loop start
					}
					sec_sequence.push_back( eSecCommand(eSecCommand::UPDATE_CURRENT_ROTORPARAMS) );
					sec_sequence.push_back( eSecCommand(eSecCommand::SET_POWER_LIMITING_MODE, eSecCommand::modeDynamic) );
					frontend.setData(3, RotorCmd);
					frontend.setData(4, sat.orbital_position);
				}
			}
			else
				csw = band;

			frontend.setData(0, csw);
			frontend.setData(1, ucsw);
			frontend.setData(2, di_param.m_toneburst_param);

			if (!linked && doSetVoltageToneFrontend)
			{
				eSecCommand::pair compare;
				compare.voltage = voltage;
				compare.steps = +3;
				sec_sequence.push_back( eSecCommand(eSecCommand::IF_VOLTAGE_GOTO, compare) ); // voltage already correct ?
				sec_sequence.push_back( eSecCommand(eSecCommand::SET_VOLTAGE, voltage) );
				sec_sequence.push_back( eSecCommand(eSecCommand::SLEEP, 10) );

				sec_sequence.push_back( eSecCommand(eSecCommand::SET_TONE, tone) );
				sec_sequence.push_back( eSecCommand(eSecCommand::SLEEP, 15) );
			}

			if (doSetFrontend)
			{
				sec_sequence.push_back( eSecCommand(eSecCommand::START_TUNE_TIMEOUT) );
				sec_sequence.push_back( eSecCommand(eSecCommand::SET_FRONTEND) );
			}
			frontend.setSecSequence(sec_sequence);

			return 0;
		}
	}

	eDebug("found no useable satellite configuration for orbital position (%d)", sat.orbital_position );
	return -1;
}

RESULT eDVBSatelliteEquipmentControl::clear()
{
	for (int i=0; i <= m_lnbidx; ++i)
	{
		m_lnbs[i].m_satellites.clear();
		m_lnbs[i].tuner_mask = 0;
	}
	m_lnbidx=-1;

// clear linked tuner configuration
	for (eSmartPtrList<eDVBRegisteredFrontend>::iterator it(m_avail_frontends.begin()); it != m_avail_frontends.end(); ++it)
		it->m_frontend->setData(7, -1);

	return 0;
}

// helper function for setTunerLinked and setTunerDepends
RESULT eDVBSatelliteEquipmentControl::setDependencyPointers( int tu1, int tu2, int dest_data_byte )
{
	if (tu1 == tu2)
		return -1;

	eDVBRegisteredFrontend *p1=NULL, *p2=NULL;

	int cnt=0;
	for (eSmartPtrList<eDVBRegisteredFrontend>::iterator it(m_avail_frontends.begin()); it != m_avail_frontends.end(); ++it, ++cnt)
	{
		if (cnt == tu1)
			p1 = *it;
		else if (cnt == tu2)
			p2 = *it;
	}
	if (p1 && p2)
	{
		p1->m_frontend->setData(dest_data_byte, (int)p2);  // this is evil..
		p2->m_frontend->setData(dest_data_byte, (int)p1);
		return 0;
	}
	return -1;
}

/* LNB Specific Parameters */
RESULT eDVBSatelliteEquipmentControl::addLNB()
{
	if ( (m_lnbidx+1) < (int)(sizeof(m_lnbs) / sizeof(eDVBSatelliteLNBParameters)))
		m_curSat=m_lnbs[++m_lnbidx].m_satellites.end();
	else
	{
		eDebug("no more LNB free... cnt is %d", m_lnbidx);
		return -ENOSPC;
	}
	return 0;
}

RESULT eDVBSatelliteEquipmentControl::setLNBTunerMask(int tunermask)
{
	if ( currentLNBValid() )
		m_lnbs[m_lnbidx].tuner_mask = tunermask;
	else
		return -ENOENT;
	return 0;
}

RESULT eDVBSatelliteEquipmentControl::setLNBLOFL(int lofl)
{
	if ( currentLNBValid() )
		m_lnbs[m_lnbidx].m_lof_lo = lofl;
	else
		return -ENOENT;
	return 0;
}

RESULT eDVBSatelliteEquipmentControl::setLNBLOFH(int lofh)
{
	if ( currentLNBValid() )
		m_lnbs[m_lnbidx].m_lof_hi = lofh;
	else
		return -ENOENT;
	return 0;
}

RESULT eDVBSatelliteEquipmentControl::setLNBThreshold(int threshold)
{
	if ( currentLNBValid() )
		m_lnbs[m_lnbidx].m_lof_threshold = threshold;
	else
		return -ENOENT;
	return 0;
}

RESULT eDVBSatelliteEquipmentControl::setLNBIncreasedVoltage(bool onoff)
{
	if ( currentLNBValid() )
		m_lnbs[m_lnbidx].m_increased_voltage = onoff;
	else
		return -ENOENT;
	return 0;
}

/* DiSEqC Specific Parameters */
RESULT eDVBSatelliteEquipmentControl::setDiSEqCMode(int diseqcmode)
{
	if ( currentLNBValid() )
		m_lnbs[m_lnbidx].m_diseqc_parameters.m_diseqc_mode = (eDVBSatelliteDiseqcParameters::t_diseqc_mode)diseqcmode;
	else
		return -ENOENT;
	return 0;
}

RESULT eDVBSatelliteEquipmentControl::setToneburst(int toneburst)
{
	if ( currentLNBValid() )
		m_lnbs[m_lnbidx].m_diseqc_parameters.m_toneburst_param = (eDVBSatelliteDiseqcParameters::t_toneburst_param)toneburst;
	else
		return -ENOENT;
	return 0;
}

RESULT eDVBSatelliteEquipmentControl::setRepeats(int repeats)
{
	if ( currentLNBValid() )
		m_lnbs[m_lnbidx].m_diseqc_parameters.m_repeats=repeats;
	else
		return -ENOENT;
	return 0;
}

RESULT eDVBSatelliteEquipmentControl::setCommittedCommand(int command)
{
	if ( currentLNBValid() )
		m_lnbs[m_lnbidx].m_diseqc_parameters.m_committed_cmd=command;
	else
		return -ENOENT;
	return 0;
}

RESULT eDVBSatelliteEquipmentControl::setUncommittedCommand(int command)
{
	if ( currentLNBValid() )
		m_lnbs[m_lnbidx].m_diseqc_parameters.m_uncommitted_cmd = command;
	else
		return -ENOENT;
	return 0;
}

RESULT eDVBSatelliteEquipmentControl::setCommandOrder(int order)
{
	if ( currentLNBValid() )
		m_lnbs[m_lnbidx].m_diseqc_parameters.m_command_order=order;
	else
		return -ENOENT;
	return 0;
}

RESULT eDVBSatelliteEquipmentControl::setFastDiSEqC(bool onoff)
{
	if ( currentLNBValid() )
		m_lnbs[m_lnbidx].m_diseqc_parameters.m_use_fast=onoff;
	else
		return -ENOENT;
	return 0;
}

RESULT eDVBSatelliteEquipmentControl::setSeqRepeat(bool onoff)
{
	if ( currentLNBValid() )
		m_lnbs[m_lnbidx].m_diseqc_parameters.m_seq_repeat = onoff;
	else
		return -ENOENT;
	return 0;
}

/* Rotor Specific Parameters */
RESULT eDVBSatelliteEquipmentControl::setLongitude(float longitude)
{
	if ( currentLNBValid() )
		m_lnbs[m_lnbidx].m_rotor_parameters.m_gotoxx_parameters.m_longitude=longitude;
	else
		return -ENOENT;
	return 0;
}

RESULT eDVBSatelliteEquipmentControl::setLatitude(float latitude)
{
	if ( currentLNBValid() )
		m_lnbs[m_lnbidx].m_rotor_parameters.m_gotoxx_parameters.m_latitude=latitude;
	else
		return -ENOENT;
	return 0;
}

RESULT eDVBSatelliteEquipmentControl::setLoDirection(int direction)
{
	if ( currentLNBValid() )
		m_lnbs[m_lnbidx].m_rotor_parameters.m_gotoxx_parameters.m_lo_direction=direction;
	else
		return -ENOENT;
	return 0;
}

RESULT eDVBSatelliteEquipmentControl::setLaDirection(int direction)
{
	if ( currentLNBValid() )
		m_lnbs[m_lnbidx].m_rotor_parameters.m_gotoxx_parameters.m_la_direction=direction;
	else
		return -ENOENT;
	return 0;
}

RESULT eDVBSatelliteEquipmentControl::setUseInputpower(bool onoff)
{
	if ( currentLNBValid() )
		m_lnbs[m_lnbidx].m_rotor_parameters.m_inputpower_parameters.m_use=onoff;
	else
		return -ENOENT;
	return 0;
}

RESULT eDVBSatelliteEquipmentControl::setInputpowerDelta(int delta)
{
	if ( currentLNBValid() )
		m_lnbs[m_lnbidx].m_rotor_parameters.m_inputpower_parameters.m_delta=delta;
	else
		return -ENOENT;
	return 0;
}

/* Satellite Specific Parameters */
RESULT eDVBSatelliteEquipmentControl::addSatellite(int orbital_position)
{
	if ( currentLNBValid() )
	{
		std::map<int, eDVBSatelliteSwitchParameters>::iterator it =
			m_lnbs[m_lnbidx].m_satellites.find(orbital_position);
		if ( it == m_lnbs[m_lnbidx].m_satellites.end() )
		{
			std::pair<std::map<int, eDVBSatelliteSwitchParameters>::iterator, bool > ret =
				m_lnbs[m_lnbidx].m_satellites.insert(
					std::pair<int, eDVBSatelliteSwitchParameters>(orbital_position, eDVBSatelliteSwitchParameters())
				);
			if ( ret.second )
				m_curSat = ret.first;
			else
				return -ENOMEM;
		}
		else
			return -EEXIST;
	}
	else
		return -ENOENT;
	return 0;
}

RESULT eDVBSatelliteEquipmentControl::setVoltageMode(int mode)
{
	if ( currentLNBValid() && m_curSat != m_lnbs[m_lnbidx].m_satellites.end() )
		m_curSat->second.m_voltage_mode = (eDVBSatelliteSwitchParameters::t_voltage_mode)mode;
	else
		return -ENOENT;
	return 0;

}

RESULT eDVBSatelliteEquipmentControl::setToneMode(int mode)
{
	if ( currentLNBValid() )
	{
		if ( m_curSat != m_lnbs[m_lnbidx].m_satellites.end() )
			m_curSat->second.m_22khz_signal = (eDVBSatelliteSwitchParameters::t_22khz_signal)mode;
		else
			return -EPERM;
	}
	else
		return -ENOENT;
	return 0;
}

RESULT eDVBSatelliteEquipmentControl::setRotorPosNum(int rotor_pos_num)
{
	if ( currentLNBValid() )
	{
		if ( m_curSat != m_lnbs[m_lnbidx].m_satellites.end() )
			m_curSat->second.m_rotorPosNum=rotor_pos_num;
		else
			return -EPERM;
	}
	else
		return -ENOENT;
	return 0;
}

struct sat_compare
{
	int orb_pos, lofl, lofh;
	sat_compare(int o, int lofl, int lofh)
		:orb_pos(o), lofl(lofl), lofh(lofh)
	{}
	sat_compare(const sat_compare &x)
		:orb_pos(x.orb_pos), lofl(x.lofl), lofh(x.lofh)
	{}
	bool operator < (const sat_compare & cmp) const
	{
		if (orb_pos == cmp.orb_pos)
		{
			if ( abs(lofl-cmp.lofl) < 200000 )
			{
				if (abs(lofh-cmp.lofh) < 200000)
					return false;
				return lofh<cmp.lofh;
			}
			return lofl<cmp.lofl;
		}
		return orb_pos < cmp.orb_pos;
	}
};

PyObject *eDVBSatelliteEquipmentControl::get_exclusive_satellites(int tu1, int tu2)
{
	PyObject *ret=0;

	int tu1_mask = 1 << tu1,
		tu2_mask = 1 << tu2;

	if (tu1 != tu2)
	{
		eDVBRegisteredFrontend *p1=NULL, *p2=NULL;
		int cnt=0;
		for (eSmartPtrList<eDVBRegisteredFrontend>::iterator it(m_avail_frontends.begin()); it != m_avail_frontends.end(); ++it, ++cnt)
		{
			if (cnt == tu1)
				p1 = *it;
			else if (cnt == tu2)
				p2 = *it;
		}
		if (p1 && p2)
		{
			// check for linked tuners
			int tmp1, tmp2;
			p1->m_frontend->getData(7, tmp1);
			p2->m_frontend->getData(7, tmp2);
			if ((void*)tmp1 != p2 && (void*)tmp2 != p1)
			{
				// check for rotor dependency
				p1->m_frontend->getData(8, tmp1);
				p2->m_frontend->getData(8, tmp2);
				if ((void*)tmp1 != p2 && (void*)tmp2 != p1)
				{
					std::set<sat_compare> tu1sats, tu2sats;
					std::list<sat_compare> tu1difference, tu2difference;
					std::insert_iterator<std::list<sat_compare> > insert1(tu1difference, tu1difference.begin()),
						insert2(tu2difference, tu2difference.begin());
					for (int idx=0; idx <= m_lnbidx; ++idx )
					{
						eDVBSatelliteLNBParameters &lnb_param = m_lnbs[idx];
						for (std::map<int, eDVBSatelliteSwitchParameters>::iterator sit(lnb_param.m_satellites.begin());
							sit != lnb_param.m_satellites.end(); ++sit)
						{
							if ( lnb_param.tuner_mask & tu1_mask )
								tu1sats.insert(sat_compare(sit->first, lnb_param.m_lof_lo, lnb_param.m_lof_hi));
							if ( lnb_param.tuner_mask & tu2_mask )
								tu2sats.insert(sat_compare(sit->first, lnb_param.m_lof_lo, lnb_param.m_lof_hi));
						}
					}
					std::set_difference(tu1sats.begin(), tu1sats.end(),
						tu2sats.begin(), tu2sats.end(),
						insert1);
					std::set_difference(tu2sats.begin(), tu2sats.end(),
						tu1sats.begin(), tu1sats.end(),
						insert2);
					if (!tu1sats.empty() || !tu2sats.empty())
					{
						int idx=0;
						ret = PyList_New(2+tu1difference.size()+tu2difference.size());

						PyList_SET_ITEM(ret, idx++, PyInt_FromLong(tu1difference.size()));
						for(std::list<sat_compare>::iterator it(tu1difference.begin()); it != tu1difference.end(); ++it)
							PyList_SET_ITEM(ret, idx++, PyInt_FromLong(it->orb_pos));

						PyList_SET_ITEM(ret, idx++, PyInt_FromLong(tu2difference.size()));
						for(std::list<sat_compare>::iterator it(tu2difference.begin()); it != tu2difference.end(); ++it)
							PyList_SET_ITEM(ret, idx++, PyInt_FromLong(it->orb_pos));
					}
				}
			}
		}
	}
	if (!ret)
	{
		ret = PyList_New(2);
		PyList_SET_ITEM(ret, 0, PyInt_FromLong(0));
		PyList_SET_ITEM(ret, 1, PyInt_FromLong(0));
	}
	return ret;
}

RESULT eDVBSatelliteEquipmentControl::setTunerLinked(int tu1, int tu2)
{
	return setDependencyPointers(tu1, tu2, 7);
}

RESULT eDVBSatelliteEquipmentControl::setTunerDepends(int tu1, int tu2)
{
	return setDependencyPointers(tu1, tu2, 8);
}

bool eDVBSatelliteEquipmentControl::isRotorMoving()
{
	return m_rotorMoving;
}

void eDVBSatelliteEquipmentControl::setRotorMoving(bool b)
{
	m_rotorMoving=b;
}
