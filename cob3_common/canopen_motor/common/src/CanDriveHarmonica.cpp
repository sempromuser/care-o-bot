/****************************************************************
 *
 * Copyright (c) 2010
 *
 * Fraunhofer Institute for Manufacturing Engineering	
 * and Automation (IPA)
 *
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * Project name: care-o-bot
 * ROS stack name: cob3_common
 * ROS package name: canopen_motor
 * Description:
 *								
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *			
 * Author: Christian Connette, email:christian.connette@ipa.fhg.de
 * Supervised by: Christian Connette, email:christian.connette@ipa.fhg.de
 *
 * Date of creation: Feb 2009
 * ToDo: - Assign Adsress of digital input for homing switch "iHomeDigIn" via parameters (in evalReceived Message, Line 116).
 *       - Homing Event should be defined by a parameterfile and handed to CanDrive... e.g. via the DriveParam.h (in inithoming, Line 531).
 *		 - Check whether "requestStatus" can/should be done in the class implementing the component
 *
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Fraunhofer Institute for Manufacturing 
 *       Engineering and Automation (IPA) nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License LGPL as 
 * published by the Free Software Foundation, either version 3 of the 
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License LGPL for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public 
 * License LGPL along with this program. 
 * If not, see <http://www.gnu.org/licenses/>.
 *
 ****************************************************************/


#include <assert.h>
#include <CanDriveHarmonica.h>

//-----------------------------------------------
CanDriveHarmonica::CanDriveHarmonica()
{
	// Parameter
	m_Param.iDivForRequestStatus = 10;
	m_Param.dCanTimeout = 6;

	// Variables
	m_pCanCtrl = NULL;

	m_iStatusCtrl = 0;
	m_dPosGearMeasRad = 0;
	m_dAngleGearRadMem  = 0;
	m_dVelGearMeasRadS = 0;

	m_VelCalcTime.SetNow();

	m_bLimSwLeft = false;
	m_bLimSwRight = false;

	m_bLimitSwitchEnabled = false;

	m_iCountRequestDiv = 0;

	m_iMotorState = ST_PRE_INITIALIZED;
	m_bCurrentLimitOn = false;

	m_iNumAttempsRecFail = 0;

	m_SendTime.SetNow();
	m_StartTime.SetNow();

	m_bOutputOfFailure = false;
	
	m_bIsInitialized = false;


}

//-----------------------------------------------
void CanDriveHarmonica::setCanOpenParam( int iTxPDO1, int iTxPDO2, int iRxPDO2, int iTxSDO, int iRxSDO )
{
	m_ParamCanOpen.iTxPDO1 = iTxPDO1;
	m_ParamCanOpen.iTxPDO2 = iTxPDO2;
	m_ParamCanOpen.iRxPDO2 = iRxPDO2;
	m_ParamCanOpen.iTxSDO = iTxSDO;
	m_ParamCanOpen.iRxSDO = iRxSDO;

}

//-----------------------------------------------
bool CanDriveHarmonica::evalReceivedMsg(CanMsg& msg)
{
	bool bRet = false;
	int iDigIn;
	int iFailure;
	int iPara;

	int iHomeDigIn = 0x0001; // 0x0001 for CoB3 steering drive homing input; 0x0400 for Scara
	int iTemp1, iTemp2;
	
	m_CanMsgLast = msg;

	//-----------------------
	// eval answers from PDO1 - transmitted on SYNC msg
	if (msg.m_iID == m_ParamCanOpen.iTxPDO1)
	{
		iTemp1 = (msg.getAt(3) << 24) | (msg.getAt(2) << 16)
				| (msg.getAt(1) << 8) | (msg.getAt(0) );

		m_dPosGearMeasRad = m_DriveParam.getSign() * m_DriveParam.
			PosMotIncrToPosGearRad(iTemp1);

		iTemp2 = (msg.getAt(7) << 24) | (msg.getAt(6) << 16)
				| (msg.getAt(5) << 8) | (msg.getAt(4) );
		
		m_dVelGearMeasRadS = m_DriveParam.getSign() * m_DriveParam.
			VelMotIncrPeriodToVelGearRadS(iTemp2);

		m_WatchdogTime.SetNow();
	}	
	
	//-----------------------
	// eval answers from binary interpreter
	if (msg.m_iID == m_ParamCanOpen.iTxPDO2)
	{
		if( (msg.getAt(0) == 'P') && (msg.getAt(1) == 'X') ) // current pos
		{
		}

		else if( (msg.getAt(0) == 'P') && (msg.getAt(1) == 'A') ) // position absolute
		{
		}

		else if( (msg.getAt(0) == 'J') && (msg.getAt(1) == 'V') ) // current velocity
		{
		}

		else if( (msg.getAt(0) == 'B') && (msg.getAt(1) == 'G') ) // begin motion
		{
		}

		else if( (msg.getAt(0) == 'U') && (msg.getAt(1) == 'M') ) // user mode
		{
			iDigIn = 0x1FFFFF & ( (msg.getAt(7) << 24) | (msg.getAt(6) << 16)
				| (msg.getAt(5) << 8) | (msg.getAt(4)) );
		}

		else if( (msg.getAt(0) == 'I') && (msg.getAt(1) == 'P') ) // digital in == limit switches
		{
			iDigIn = 0x1FFFFF & ( (msg.getAt(7) << 24) | (msg.getAt(6) << 16)
				| (msg.getAt(5) << 8) | (msg.getAt(4)) );
			iDigIn = 0x1FFFFF & ( (msg.getAt(7) << 24) | (msg.getAt(6) << 16)
				| (msg.getAt(5) << 8) | (msg.getAt(4)) );

			if( (iDigIn & iHomeDigIn) != 0x0000 )
			{
				m_bLimSwRight = true;
			}			
		}

		else if( (msg.getAt(0) == 'S') && (msg.getAt(1) == 'R') ) // status
		{
			m_iStatusCtrl = (msg.getAt(7) << 24) | (msg.getAt(6) << 16)
				| (msg.getAt(5) << 8) | (msg.getAt(4) );

			evalStatusRegister(m_iStatusCtrl);
		}

		else if( (msg.getAt(0) == 'M') && (msg.getAt(1) == 'F') ) // motor failure
		{
			iFailure = (msg.getAt(7) << 24) | (msg.getAt(6) << 16)
				| (msg.getAt(5) << 8) | (msg.getAt(4) );

			evalMotorFailure(iFailure);
		}

		// debug eval
		else if( (msg.getAt(0) == 'U') && (msg.getAt(1) == 'M') )
		{
			iPara = (msg.getAt(7) << 24) | (msg.getAt(6) << 16)
				| (msg.getAt(5) << 8) | (msg.getAt(4) );

			std::cout << "um " << iPara << std::endl;
		}
		
		else if( (msg.getAt(0) == 'P') && (msg.getAt(1) == 'M') )
		{
			iPara = (msg.getAt(7) << 24) | (msg.getAt(6) << 16)
				| (msg.getAt(5) << 8) | (msg.getAt(4) );

			std::cout << "pm " << iPara << std::endl;
		}
		
		else if( (msg.getAt(0) == 'A') && (msg.getAt(1) == 'C') )
		{
			iPara = (msg.getAt(7) << 24) | (msg.getAt(6) << 16)
				| (msg.getAt(5) << 8) | (msg.getAt(4) );

			std::cout << "ac " << iPara << std::endl;
		}
		
		else if( (msg.getAt(0) == 'D') && (msg.getAt(1) == 'C') )
		{
			iPara = (msg.getAt(7) << 24) | (msg.getAt(6) << 16)
				| (msg.getAt(5) << 8) | (msg.getAt(4) );

			std::cout << "dc " << iPara << std::endl;
		}
		else if( (msg.getAt(0) == 'H') && (msg.getAt(1) == 'M') )
		{
			// status message (homing armed = 1 / disarmed = 0) is encoded in 5th byte
			if(msg.getAt(4) == 0)
			{
				// if 0 received: elmo disarmed homing after receiving the defined event
				m_bLimSwRight = true;
			}
		}
		else if( (msg.getAt(0) == 'I') && (msg.getAt(1) == 'Q') )
		{
			int iVal=0;
			iVal = (msg.getAt(7) << 24) | (msg.getAt(6) << 16)
				| (msg.getAt(5) << 8) | (msg.getAt(4) );
			float* pfVal;
			pfVal=(float*)&iVal;
			m_dMotorCurr = *pfVal;			
		}		
		else
		{
		}

		m_WatchdogTime.SetNow();

		bRet = true;
	}
	
	//-----------------------
	// eval answer from SDO
	if (msg.m_iID == m_ParamCanOpen.iTxSDO)
	{
		m_WatchdogTime.SetNow();

		bRet = true;
	}

	return bRet;
}

//-----------------------------------------------
bool CanDriveHarmonica::init()
{
	int iCnt, iPosCnt;
	bool bRet = true;
	CanMsg Msg;

	m_iMotorState = ST_PRE_INITIALIZED;


	// Set Values for Modulo-Counting. Neccessary to preserve absolute position for homed motors (after encoder overflow)
	int iIncrRevWheel = int( (double)m_DriveParam.getGearRatio() * (double)m_DriveParam.getBeltRatio()
			        * (double)m_DriveParam.getEncIncrPerRevMot() * 3 );
	IntprtSetInt(8, 'M', 'O', 0, 0);
	usleep(20000);
	IntprtSetInt(8, 'X', 'M', 2, iIncrRevWheel * 5000);
	usleep(20000);
	IntprtSetInt(8, 'X', 'M', 1, -iIncrRevWheel * 5000);
	usleep(20000);
	

	setTypeMotion(MOTIONTYPE_VELCTRL);
	// ---------- set position counter to zero
	IntprtSetInt(8, 'P', 'X', 0, 0);

	iCnt = 0;
	while(true)
	{
		m_pCanCtrl->receiveMsg(&Msg);
	
		if( (Msg.getAt(0) == 'P') && (Msg.getAt(1) == 'X') )
		{
			iPosCnt = (Msg.getAt(7) << 24) | (Msg.getAt(6) << 16)
				| (Msg.getAt(5) << 8) | (Msg.getAt(4) );
			
			m_dPosGearMeasRad = m_DriveParam.getSign() * m_DriveParam.PosMotIncrToPosGearRad(iPosCnt);
			m_dAngleGearRadMem  = m_dPosGearMeasRad;
			break;
		}

		if ( iCnt > 300 )
		{
			std::cout << "CanDriveHarmonica: initial position not set" << std::endl;
			bRet = false;
			break;
		}

		usleep(10000);
		iCnt++;
	}

	// ---------- set PDO mapping
	// Mapping of TPDO1:
	// - position
	// - velocity
	
	// stop all emissions of TPDO1
	sendSDODownload(0x1A00, 0, 0);
	
	// position 4 byte of TPDO1
	sendSDODownload(0x1A00, 1, 0x60640020);

	// velocity 4 byte of TPDO1
	sendSDODownload(0x1A00, 2, 0x60690020);
	
	// transmission type "synch"
	sendSDODownload(0x1800, 2, 1);
	
	// activate mapped objects
	sendSDODownload(0x1A00, 0, 2);

	m_bWatchdogActive = false;
	
	if( bRet )
		 m_bIsInitialized = true;
	 
	return bRet;	
}
//-----------------------------------------------
bool CanDriveHarmonica::stop()
{	
	bool bRet = true;
	// motor off
	IntprtSetInt(8, 'M', 'O', 0, 0);
	usleep(20000);
	return bRet;
}
//-----------------------------------------------
bool CanDriveHarmonica::start()
{
	// motor on
	IntprtSetInt(8, 'M', 'O', 0, 1);
	usleep(20000);

	// ------------------- request status
	int iCnt;
	bool bRet = true;
	int iStatus;
	CanMsg Msg;

	//  clear the can buffer 
	do
	{
		bRet = m_pCanCtrl->receiveMsg(&Msg);
	}
	while(bRet == true);

	// send request
	IntprtSetInt(4, 'S', 'R', 0, 0);
	
	iCnt = 0;
	while(true)
	{
		m_pCanCtrl->receiveMsg(&Msg);
	
		if( (Msg.getAt(0) == 'S') && (Msg.getAt(1) == 'R') )
		{
			iStatus = (Msg.getAt(7) << 24) | (Msg.getAt(6) << 16)
				| (Msg.getAt(5) << 8) | (Msg.getAt(4) );
			
			bRet = evalStatusRegister(iStatus);
			break;
		}

		if ( iCnt > 300 )
		{
			std::cout << "CanDriveHarmonica::enableMotor(): No answer on status request" << std::endl;
			bRet = false;
			break;
		}

		usleep(10000);
		iCnt++;
	}

	// ------------------- start watchdog timer
	m_WatchdogTime.SetNow();
	m_SendTime.SetNow();

	return bRet;
}

//-----------------------------------------------
bool CanDriveHarmonica::reset()
{
	// repeat initialization
	
	// start network
	CanMsg msg;
	msg.m_iID  = 0;
	msg.m_iLen = 2;
	msg.set(1,0,0,0,0,0,0,0);
	m_pCanCtrl->transmitMsg(msg);

	// init and start
	bool bRet = init();
	bRet |= start();

	return bRet;
}
//-----------------------------------------------
bool CanDriveHarmonica::shutdown()
{
	std::cout << "shutdown drive " << m_DriveParam.getDriveIdent() << std::endl;

	IntprtSetInt(8, 'M', 'O', 0, 0);

	return true;
}

//-----------------------------------------------
bool CanDriveHarmonica::startWatchdog(bool bStarted)
{
	if (bStarted == true)
	{
		//save Watchdog state into member variable
		m_bWatchdogActive = true;
		// ------- init watchdog
		// Harmonica checks PC hearbeat
		// note: the COB-ID for a heartbeat message = 0x700 + Device ID
		
		const int c_iHeartbeatTimeMS = 1000;
		const int c_iNMTNodeID = 0x00;
		
		// consumer (PC) heartbeat time
		sendSDODownload(0x1016, 1, (c_iNMTNodeID << 16) | c_iHeartbeatTimeMS);
 		
		// error behavior after failure: 0=pre-operational, 1=no state change, 2=stopped"	
		sendSDODownload(0x1029, 1, 2);
		
		// motor behavior after heartbeat failre: "quick stop"
		sendSDODownload(0x6007, 0, 3);

		// acivate emergency events: "heartbeat event"
		// Object 0x2F21 = "Emergency Events" which cause an Emergency Message
		// Bit 3 is responsible for Heartbeart-Failure.--> Hex 0x08
		sendSDODownload(0x2F21, 0, 0x08);
		usleep(20000);
 
	}
	else
	{	
		//save Watchdog state into member variable
		m_bWatchdogActive = false;

		//Motor action after Hearbeat-Error: No Action		
		sendSDODownload(0x6007, 0, 0);

		//Error Behavior: No state change
		sendSDODownload(0x1029, 1, 1);

		// Deacivate emergency events: "heartbeat event"
		// Object 0x2F21 = "Emergency Events" which cause an Emergency Message
		// Bit 3 is responsible for Heartbeart-Failure.
		sendSDODownload(0x2F21, 0, 0x00);
		usleep(25000);
			
		
	}

	return true;
}

//-----------------------------------------------
bool CanDriveHarmonica::disableBrake(bool bDisabled)
{
	return true;
}

//-----------------------------------------------
double CanDriveHarmonica::getTimeToLastMsg()
{
	m_CurrentTime.SetNow();

	return m_CurrentTime - m_WatchdogTime;
}
//-----------------------------------------------
bool CanDriveHarmonica::getStatusLimitSwitch()
{
	return m_bLimSwRight;
}
//-----------------------------------------------
bool CanDriveHarmonica::initHoming()
{
	const int c_iPosRef = m_DriveParam.getEncOffset(); 
	
		// 1. make sure that, if on elmo controller still a pending homing from a previous startup is running (in case of warm-start without switching of the whole robot), this old sequence is disabled
		// disarm homing process
		IntprtSetInt(8, 'H', 'M', 1, 0);

		// always give can and controller some time to understand the command
		usleep(20000);
	
		// 2. configure the homing sequence
		// 2.a set the value to which the increment counter shall be reseted as soon as the homing event occurs
		// value to load at homing event
		IntprtSetInt(8, 'H', 'M', 2, c_iPosRef);
		usleep(20000);
		
		// 2.b choose the chanel/switch on which the controller listens for a change or defined logic level (the homing event) (high/low/falling/rising)
		// home event
		// iHomeEvent = 5 : event according to defined FLS switch (for scara arm)
		// iHomeEvent = 9 : event according to definded DIN1 switch (for full steerable wheels COb3)
		// ToDo: this should be defined by a parameterfile and handed to CanDrive... e.g. via the DriveParam.h
		IntprtSetInt(8, 'H', 'M', 3, 9);
		usleep(20000);


		// 2.c choose the action that the controller shall perform after the homing event occured
		// HM[4] = 0 : after Event stop immediately
		// HM[4] = 2 : Do nothing!
		IntprtSetInt(8, 'H', 'M', 4, 2);
		usleep(20000);

		// 2.d choose the setting of the position counter (i.e. to the value defined in 2.a) after the homing event occured
		// HM[5] = 0 : absolute setting of position counter: PX = HM[2]
		IntprtSetInt(8, 'H', 'M', 5, 0);
		usleep(20000);

		// 3. let the motor turn some time to give him the possibility to escape the approximation sensor if accidently in home position already at the beginning of the sequence (done in CanCtrlPltf...)

	return true;	
}


//-----------------------------------------------
bool CanDriveHarmonica::execHoming()
{
	int iCnt;
	CanMsg Msg;
	bool bRet = true;

	int iNrDrive = m_DriveParam.getDriveIdent();

	// 4. arm the homing process -> as soon as the approximation sensor is reached and the homing event occurs the commands set in 2. take effect
	// arm homing process
	IntprtSetInt(8, 'H', 'M', 1, 1);
		
	// 5. clear the can buffer to get rid of all uneccessary and potentially disturbing commands still floating through the wires
	do
	{
		// read from can
		bRet = m_pCanCtrl->receiveMsg(&Msg);
	}
	while(bRet == true);

	// 6. now listen for status of homing, to synchronize program flow -> proceed only after homing was succesful (homing disarmed by elmo) or timeout occured

	// set timeout counter to zero
	iCnt = 0;

	do
	{
		// 6.a ask for status of homing process (armed/disarmed)
		// ask for first byte in Homing Configuration
		IntprtSetInt(4, 'H', 'M', 1, 0);

		// 6.b read message from can
		m_pCanCtrl->receiveMsgRetry(&Msg, 10);
		
		// 6.c see if received message is answer of request and if so what is the status
		if( (Msg.getAt(0) == 'H') && (Msg.getAt(1) == 'M') )
		{	
			// status message (homing armed = 1 / disarmed = 0) is encoded in 5th byte
			if(Msg.getAt(4) == 0)
			{
				// if 0 received: elmo disarmed homing after receiving the defined event
				m_bLimSwRight = true;
				break;
			}
		}

		// increase count for timeout
		usleep(10000);
		iCnt++;

	}
	while((m_bLimSwRight == false) && (iCnt<1000)); // wait some time
	
	// 7. see why finished (homed or timeout) and log out
	if(iCnt>=1000)
	{
		std::cout << "Homing failed - limit switch " << iNrDrive << " not reached" << std::endl;
	}
	else
	{
		std::cout << "Homing successful - limit switch " << iNrDrive << " ok" << std::endl;
	}

	return true;
}
//-----------------------------------------------
void CanDriveHarmonica::setGearPosVelRadS(double dPosGearRad, double dVelGearRadS)
{
	int iPosEncIncr;
	int iVelEncIncrPeriod;
		
	m_DriveParam.PosVelRadToIncr(dPosGearRad, dVelGearRadS, &iPosEncIncr, &iVelEncIncrPeriod);
		
	if(iVelEncIncrPeriod > m_DriveParam.getVelMax())
	{
		iVelEncIncrPeriod = (int)m_DriveParam.getVelMax();
	}

	if(iVelEncIncrPeriod < -m_DriveParam.getVelMax())
	{
		iVelEncIncrPeriod = (int)-m_DriveParam.getVelMax();
	}

	if(m_iTypeMotion == MOTIONTYPE_POSCTRL)
	{		
			//new: set VELOCITY for PTP Motion		
			IntprtSetInt(8, 'S', 'P', 0, iVelEncIncrPeriod);
			
			// Position Relativ ("PR") , because of positioning of driving wheel
			// which is not initialized to zero on a specific position
			// only when command is for homed steering wheel set absolute
			if (m_DriveParam.getIsSteer() == true)
				IntprtSetInt(8, 'P', 'A', 0, iPosEncIncr);
			else 
				IntprtSetInt(8, 'P', 'R', 0, iPosEncIncr);

			IntprtSetInt(4, 'B', 'G', 0, 0);
		
	}

	if(m_iTypeMotion == MOTIONTYPE_VELCTRL)
	{	
		iVelEncIncrPeriod *= m_DriveParam.getSign();
		IntprtSetInt(8, 'J', 'V', 0, iVelEncIncrPeriod);
		IntprtSetInt(4, 'B', 'G', 0, 0);
	}
	
	// request pos and vel by TPDO1, triggered by SYNC msg
	// (to request pos by SDO usesendSDOUpload(0x6064, 0) )
	CanMsg msg;
	msg.m_iID  = 0x80;
	msg.m_iLen = 0;
	msg.set(0,0,0,0,0,0,0,0);
	m_pCanCtrl->transmitMsg(msg);
}

//-----------------------------------------------
void CanDriveHarmonica::setGearVelRadS(double dVelGearRadS)
{
	int iVelEncIncrPeriod;
	
	// calc motor velocity from joint velocity
	iVelEncIncrPeriod = m_DriveParam.getSign() * m_DriveParam.VelGearRadSToVelMotIncrPeriod(dVelGearRadS);

	if(iVelEncIncrPeriod > m_DriveParam.getVelMax())
	{
		std::cout << "SteerVelo asked for " << iVelEncIncrPeriod << " EncIncrements" << std::endl;
		iVelEncIncrPeriod = (int)m_DriveParam.getVelMax();
	}

	if(iVelEncIncrPeriod < -m_DriveParam.getVelMax())
	{
		std::cout << "SteerVelo asked for " << iVelEncIncrPeriod << " EncIncrements" << std::endl;
		iVelEncIncrPeriod = -1 * (int)m_DriveParam.getVelMax();
	}
	
	IntprtSetInt(8, 'J', 'V', 0, iVelEncIncrPeriod);
	IntprtSetInt(4, 'B', 'G', 0, 0);

	// request pos and vel by TPDO1, triggered by SYNC msg
	// (to request pos by SDO usesendSDOUpload(0x6064, 0) )
	CanMsg msg;
	msg.m_iID  = 0x80;
	msg.m_iLen = 0;
	msg.set(0,0,0,0,0,0,0,0);
	m_pCanCtrl->transmitMsg(msg);

	// send heartbeat to kepp watchdog inactive
	msg.m_iID  = 0x700;
	msg.m_iLen = 5;
	msg.set(0x00,0,0,0,0,0,0,0);
	m_pCanCtrl->transmitMsg(msg);

	m_CurrentTime.SetNow();
	double dt = m_CurrentTime - m_SendTime;
	if ((dt > 1.0) && m_bWatchdogActive)
	{
		std::cout << "Time between send velocity of motor " << m_DriveParam.getDriveIdent() 
			<< " is too large: " << dt << " s" << std::endl;
	}
	m_SendTime.SetNow();


	// request status
	m_iCountRequestDiv++;
	if (m_iCountRequestDiv > m_Param.iDivForRequestStatus)
	{
		requestStatus();
		m_iCountRequestDiv = 0;
	}
}

//-----------------------------------------------
void CanDriveHarmonica::getGearPosRad(double* dGearPosRad)
{
	*dGearPosRad = m_dPosGearMeasRad;
}

//-----------------------------------------------
void CanDriveHarmonica::getGearPosVelRadS(double* pdAngleGearRad, double* pdVelGearRadS)
{
	*pdAngleGearRad = m_dPosGearMeasRad;
	*pdVelGearRadS = m_dVelGearMeasRadS;
}

//-----------------------------------------------
void CanDriveHarmonica::getGearDeltaPosVelRadS(double* pdAngleGearRad, double* pdVelGearRadS)
{
	*pdAngleGearRad = m_dPosGearMeasRad - m_dAngleGearRadMem;
	*pdVelGearRadS = m_dVelGearMeasRadS;
	m_dAngleGearRadMem = m_dPosGearMeasRad;
}

//-----------------------------------------------
void CanDriveHarmonica::getData(double* pdPosGearRad, double* pdVelGearRadS,
								int* piTorqueCtrl, int* piStatusCtrl)
{
	*pdPosGearRad = m_dPosGearMeasRad;
	*pdVelGearRadS = m_dVelGearMeasRadS;
	*piTorqueCtrl = m_iTorqueCtrl;
	*piStatusCtrl = m_iStatusCtrl;
}

//-----------------------------------------------
void CanDriveHarmonica::requestStatus()
{
	IntprtSetInt(4, 'S', 'R', 0, 0);
}

//-----------------------------------------------
bool CanDriveHarmonica::isError()
{
	if (m_iMotorState != ST_MOTOR_FAILURE)
	{
		// Check timeout of can communication
		double dWatchTime = getTimeToLastMsg();

		if (dWatchTime>m_Param.dCanTimeout)
		{
			if ( m_bOutputOfFailure == false)
			{
				std::cout << "Motor " << m_DriveParam.getDriveIdent() <<
					" has no can communiction for " << dWatchTime << " s." << std::endl;
			}

			m_iMotorState = ST_MOTOR_FAILURE;
			m_FailureStartTime.SetNow();
		}
		
	}
	
	return (m_iMotorState == ST_MOTOR_FAILURE);
}
//-----------------------------------------------
bool CanDriveHarmonica::setTypeMotion(int iType)
{
	int iMaxAcc = int(m_DriveParam.getMaxAcc());
	int iMaxDcc = int(m_DriveParam.getMaxDec());
	CanMsg Msg;

	if (iType == MOTIONTYPE_POSCTRL)
	{
		// 1.) Switch to UnitMode = 5 (Single Loop Position Control) //
	
		// switch off Motor to change Unit-Mode
		IntprtSetInt(8, 'M', 'O', 0, 0);
		usleep(20000);
		// switch Unit-Mode
		IntprtSetInt(8, 'U', 'M', 0, 5);
			
		// set Target Radius to X Increments
		IntprtSetInt(8, 'T', 'R', 1, 15);
		// set Target Time to X ms
		IntprtSetInt(8, 'T', 'R', 2, 100);

		// set maximum Acceleration to X Incr/s^2		
		IntprtSetInt(8, 'A', 'C', 0, iMaxAcc);
		// set maximum decceleration to X Incr/s^2
		IntprtSetInt(8, 'D', 'C', 0, iMaxDcc);	
		usleep(100000);
		
		
	}
    else if (iType == MOTIONTYPE_TORQUECTRL)
	{
		// Switch to TorqueControll-Mode
		// switch off Motor to change Unit-Mode
		IntprtSetInt(8, 'M', 'O', 0, 0);
		usleep(50000);
		// switch Unit-Mode 1: Torque Controlled
		IntprtSetInt(8, 'U', 'M', 0, 1);
		// disable external compensation input
		// to avoid noise from that input pin
		IntprtSetInt(8, 'R', 'M', 0, 0);

		// debugging:
		std::cout << "Motor"<<m_DriveParam.getDriveIdent()<<" Unit Mode switched to: TORQUE controlled" << std::endl;
		usleep(100000);
	}
	else
	{
		//Default Motion Type = VelocityControled
		// switch off Motor to change Unit-Mode
		IntprtSetInt(8, 'M', 'O', 0, 0);
		// switch Unit-Mode
		IntprtSetInt(8, 'U', 'M', 0, 2);
		// set profiler Mode (only if Unit Mode = 2)
		IntprtSetInt(8, 'P', 'M', 0, 1);

		// set maximum Acceleration to X Incr/s^2		
		IntprtSetInt(8, 'A', 'C', 0, iMaxAcc);
		// set maximum decceleration to X Incr/s^2
		IntprtSetInt(8, 'D', 'C', 0, iMaxDcc);	
		usleep(100000);
	}
	
	m_iTypeMotion = iType;
	return true;
}


//-----------------------------------------------
void CanDriveHarmonica::IntprtSetInt(int iDataLen, char cCmdChar1, char cCmdChar2, int iIndex, int iData)
{
	char cIndex[2];
	char cInt[4];
	CanMsg CMsgTr;

	CMsgTr.m_iID = m_ParamCanOpen.iRxPDO2;	
	CMsgTr.m_iLen = iDataLen;

	cIndex[0] = iIndex;
	cIndex[1] = (iIndex >> 8) & 0x3F;  // The two MSB must be 0. Cf. DSP 301 Implementation guide p. 39.

	cInt[0] = iData;
	cInt[1] = iData >> 8;
	cInt[2] = iData >> 16;
	cInt[3] = iData >> 24;

	CMsgTr.set(cCmdChar1, cCmdChar2, cIndex[0], cIndex[1], cInt[0], cInt[1], cInt[2], cInt[3]);
	m_pCanCtrl->transmitMsg(CMsgTr);
}

//-----------------------------------------------
void CanDriveHarmonica::IntprtSetFloat(int iDataLen, char cCmdChar1, char cCmdChar2, int iIndex, float fData)
{
	char cIndex[2];
	char cFloat[4];
	CanMsg CMsgTr;
	char* pTempFloat = NULL;
	
	CMsgTr.m_iID = m_ParamCanOpen.iRxPDO2;	
	CMsgTr.m_iLen = iDataLen;

	cIndex[0] = iIndex;
    // for sending float values bit 6 has to be zero and bit 7 one (according to Elmo Implementation guide)
	cIndex[1] = (iIndex >> 8) & 0x3F;	// setting bit 6 to zero with mask 0b10111111->0xBF
	cIndex[1] = cIndex[1] | 0x80;		// setting bit 7 to one with mask 0b10000000 ->0x80

	pTempFloat = (char*)&fData;
	for( int i=0; i<4; i++ )
		cFloat[i] = pTempFloat[i];
	
	CMsgTr.set(cCmdChar1, cCmdChar2, cIndex[0], cIndex[1], cFloat[0], cFloat[1], cFloat[2], cFloat[3]);
	m_pCanCtrl->transmitMsg(CMsgTr);
}

//-----------------------------------------------
void CanDriveHarmonica::sendSDOUpload(int iObjIndex, int iObjSubIndex)
{
	CanMsg CMsgTr;
	const int ciInitUploadReq = 0x40;
	
	CMsgTr.m_iLen = 8;
	CMsgTr.m_iID = m_ParamCanOpen.iRxSDO;

	unsigned char cMsg[8];
	
	cMsg[0] = ciInitUploadReq;
	cMsg[1] = iObjIndex;
	cMsg[2] = iObjIndex >> 8;
	cMsg[3] = iObjSubIndex;
	cMsg[4] = 0x00;
	cMsg[5] = 0x00;
	cMsg[6] = 0x00;
	cMsg[7] = 0x00;

	CMsgTr.set(cMsg[0], cMsg[1], cMsg[2], cMsg[3], cMsg[4], cMsg[5], cMsg[6], cMsg[7]);
	m_pCanCtrl->transmitMsg(CMsgTr);
}

//-----------------------------------------------
void CanDriveHarmonica::sendSDODownload(int iObjIndex, int iObjSubIndex, int iData)
{
	CanMsg CMsgTr;

	const int ciInitDownloadReq = 0x20;
	const int ciNrBytesNoData = 0x00;
	const int ciExpedited = 0x02;
	const int ciDataSizeInd = 0x01;
	
	CMsgTr.m_iLen = 8;
	CMsgTr.m_iID = m_ParamCanOpen.iRxSDO;

	unsigned char cMsg[8];
	
	cMsg[0] = ciInitDownloadReq | (ciNrBytesNoData << 2) | ciExpedited | ciDataSizeInd;
	cMsg[1] = iObjIndex;
	cMsg[2] = iObjIndex >> 8;
	cMsg[3] = iObjSubIndex;
	cMsg[4] = iData;
	cMsg[5] = iData >> 8;
	cMsg[6] = iData >> 16;
	cMsg[7] = iData >> 24;

	CMsgTr.set(cMsg[0], cMsg[1], cMsg[2], cMsg[3], cMsg[4], cMsg[5], cMsg[6], cMsg[7]);
	m_pCanCtrl->transmitMsg(CMsgTr);
}

//-----------------------------------------------
void CanDriveHarmonica::evalSDO(CanMsg& CMsg, int* pIndex, int* pSubindex)
{
	*pIndex = (CMsg.getAt(2) << 8) | CMsg.getAt(1);
	*pSubindex = CMsg.getAt(3);
}

//-----------------------------------------------
int CanDriveHarmonica::getSDODataInt32(CanMsg& CMsg)
{
	int iData = (CMsg.getAt(7) << 24) | (CMsg.getAt(6) << 16) |
		(CMsg.getAt(5) << 8) | CMsg.getAt(4);

	return iData;
}

//-----------------------------------------------
double CanDriveHarmonica::estimVel(double dPos)
{
	double dVel;
	double dt;

	m_CurrentTime.SetNow();

	dt = m_CurrentTime - m_VelCalcTime;

	dVel = (dPos - m_dOldPos)/dt;

	m_dOldPos = dPos;
	m_VelCalcTime.SetNow();

	return dVel;
}
//-----------------------------------------------
bool CanDriveHarmonica::evalStatusRegister(int iStatus)
{
	bool bNoError;

	// --------- Error status
	if( isBitSet(iStatus, 0) )
	{
		// ------------ Error
		if ( m_bOutputOfFailure == false )
		{
			std::cout << "Error of drive: " << m_DriveParam.getDriveIdent() << std::endl;

			if( (iStatus & 0x0000000E) == 2)
				std::cout << "- drive error under voltage" << std::endl;

			if( (iStatus & 0x0000000E) == 4)
				std::cout << "- drive error over voltage" << std::endl;

			if( (iStatus & 0x0000000E) == 10)
				std::cout << "- drive error short circuit" << std::endl;

			if( (iStatus & 0x0000000E) == 12)
				std::cout << "- drive error overheating" << std::endl;

			// Request detailed description of failure
			IntprtSetInt(4, 'M', 'F', 0, 0);
		}

		m_iNewMotorState = ST_MOTOR_FAILURE;

		bNoError = false;
	}
	else if ( isBitSet(iStatus, 6) )
	{
		// General failure 
		if ( m_bOutputOfFailure == false )
		{
			std::cout << "Motor " << m_DriveParam.getDriveIdent() << " failure latched" << std::endl;

			// Request detailed description of failure
			IntprtSetInt(4, 'M', 'F', 0, 0);

			m_FailureStartTime.SetNow();
		}
		m_iNewMotorState = ST_MOTOR_FAILURE;

		bNoError = false;
	}
	else
	{
		// ---------- No error 
		bNoError = true;

		// Clear flag for failure output only if at least one
		// status message without error has been received.
		// Printing an error message on recovery is avoided.
		m_bOutputOfFailure = false;

		// --------- General status bits
		// check if Bit 4 (-> Motor is ON) ist set
		if( isBitSet(iStatus, 4) )
		{
			if (m_iMotorState != ST_OPERATION_ENABLED)
			{
				std::cout << "Motor " << m_DriveParam.getDriveIdent() << " operation enabled" << std::endl;
				m_FailureStartTime.SetNow();
			}

			m_iNewMotorState = ST_OPERATION_ENABLED;
		}
		else
		{
			if (m_iMotorState != ST_OPERATION_DISABLED)
			{
				std::cout << "Motor " << m_DriveParam.getDriveIdent() << " operation disabled" << std::endl;
			}

			m_iNewMotorState = ST_OPERATION_DISABLED;
		}

		// Current limit
		if( isBitSet(iStatus, 13) )
		{
			if (m_bCurrentLimitOn == false)
				std::cout << "Motor " << m_DriveParam.getDriveIdent() << "current limit on" << std::endl;

			m_bCurrentLimitOn = true;
		}
		else
			m_bCurrentLimitOn = false;
	}

	// Change state
	m_iMotorState = m_iNewMotorState;

	if (m_iMotorState == ST_MOTOR_FAILURE)
		m_bOutputOfFailure = true;

	return bNoError;
}

//-----------------------------------------------
void CanDriveHarmonica::evalMotorFailure(int iFailure)
{

	std::cout << "Motor " << m_DriveParam.getDriveIdent() << " has a failure:" << std::endl;
	
	if( isBitSet(iFailure, 2) )
	{
		std::cout << "- feedback loss" << std::endl;
	}

	if( isBitSet(iFailure, 3) )
	{
		std::cout << "- peak current excced" << std::endl;
	}

	if( isBitSet(iFailure, 7) )
	{
		std::cout << "- speed track error" << std::endl;
	}
	
	if( isBitSet(iFailure, 8) )
	{
		std::cout << "- position track error" << std::endl;
	}

	if( isBitSet(iFailure, 17) )
	{
		std::cout << "- speed limit exceeded" << std::endl;
	}
	
	if( isBitSet(iFailure, 21) )
	{
		std::cout << "- motor stuck" << std::endl;
	}
}

//-----------------------------------------------
void CanDriveHarmonica::setMotorTorque(double dTorqueNm)
{
	// convert commanded motor current into amperes
	float fMotCurr = m_DriveParam.getSign() * dTorqueNm / m_DriveParam.getCurrToTorque();

	// check for limitations
	if  (fMotCurr > m_DriveParam.getCurrMax())
	{
		fMotCurr = m_DriveParam.getCurrMax();
		std::cout << "Torque command too high: " << fMotCurr << " Nm. Torque has been limitited." << std::endl;
	}
	if (fMotCurr < -m_DriveParam.getCurrMax())
	{
		fMotCurr = -m_DriveParam.getCurrMax();
		std::cout << "Torque command too high: " << fMotCurr << " Nm. Torque has been limitited." << std::endl;
	}

	// send Command 
	IntprtSetFloat(8, 'T', 'C', 0, fMotCurr);

	// request pos and vel by TPDO1, triggered by SYNC msg
	CanMsg msg;
	msg.m_iID  = 0x80;
	msg.m_iLen = 0;
	msg.set(0,0,0,0,0,0,0,0);
	m_pCanCtrl->transmitMsg(msg);

	// send heartbeat to keep watchdog inactive
	msg.m_iID  = 0x700;
	msg.m_iLen = 5;
	msg.set(0x00,0,0,0,0,0,0,0);
	m_pCanCtrl->transmitMsg(msg);

	m_CurrentTime.SetNow();
	double dt = m_CurrentTime - m_SendTime;
	if (dt > 1.0)
	{
		std::cout << "Time between send current/torque of motor " << m_DriveParam.getDriveIdent() 
			<< " is too large: " << dt << " s" << std::endl;
	}
	m_SendTime.SetNow();


	// request status
	m_iCountRequestDiv++;
	if (m_iCountRequestDiv > m_Param.iDivForRequestStatus)
	{
		requestStatus();
		m_iCountRequestDiv = 0;
	}

}
//-----------------------------------------------
void CanDriveHarmonica::requestMotorTorque()
{	
   	// send command for requesting motor current:
 	IntprtSetInt(4, 'I', 'Q', 0, 0);	// active current
	//IntprtSetInt(4, 'I', 'D', 0, 0);	// reactive current
}
//-----------------------------------------------
void CanDriveHarmonica::getMotorTorque(double* dTorqueNm)
{
	// With motor sign:
	*dTorqueNm = m_DriveParam.getSign() * m_dMotorCurr * m_DriveParam.getCurrToTorque();
	
}
