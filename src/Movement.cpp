#include "Movement.h"
#include "Debug.h"
#include "Config.h"

static Movement *instance;

Movement *Movement::getInstance()
{
  if (!instance)
  {
    instance = new Movement();
  };
  return instance;
};

void Movement::reportEncoders()
{
  Serial.print(COMM_REPORT_ENCODER_SCALED);
  Serial.print(" X");
  Serial.print((float)encoderX.currentPosition() / (float)stepsPerMm[0]);
  Serial.print(" Y");
  Serial.print((float)encoderY.currentPosition() / (float)stepsPerMm[1]);
  Serial.print(" Z");
  Serial.print((float)encoderZ.currentPosition() / (float)stepsPerMm[2]);
  CurrentState::getInstance()->printQAndNewLine();

  Serial.print(COMM_REPORT_ENCODER_RAW);
  Serial.print(" X");
  Serial.print(encoderX.currentPositionRaw());
  Serial.print(" Y");
  Serial.print(encoderY.currentPositionRaw());
  Serial.print(" Z");
  Serial.print(encoderZ.currentPositionRaw());
  CurrentState::getInstance()->printQAndNewLine();

}

void Movement::getEncoderReport()
{
  serialBuffer += COMM_REPORT_ENCODER_SCALED;
  serialBuffer += " X";
  serialBuffer += (float)encoderX.currentPosition() / (float)stepsPerMm[0];
  serialBuffer += " Y";
  serialBuffer += (float)encoderY.currentPosition() / (float)stepsPerMm[1];
  serialBuffer += " Z";
  serialBuffer += (float)encoderZ.currentPosition() / (float)stepsPerMm[2];
  serialBuffer += CurrentState::getInstance()->getQAndNewLine();

  serialBuffer += COMM_REPORT_ENCODER_RAW;
  serialBuffer += " X";
  serialBuffer += encoderX.currentPositionRaw();
  serialBuffer += " Y";
  serialBuffer += encoderY.currentPositionRaw();
  serialBuffer += " Z";
  serialBuffer += encoderZ.currentPositionRaw();
  serialBuffer += CurrentState::getInstance()->getQAndNewLine();

}

void Movement::reportStatus(MovementAxis *axis, int axisStatus)
{  
  serialBuffer += COMM_REPORT_CMD_STATUS;
  serialBuffer += " ";
  serialBuffer += axis->channelLabel;
  serialBuffer += axisStatus;
  serialBuffer += CurrentState::getInstance()->getQAndNewLine();

  //Serial.print(COMM_REPORT_CMD_STATUS);
  //Serial.print(" ");
  //Serial.print(axis->label);
  //Serial.print(axisStatus);
  //CurrentState::getInstance()->printQAndNewLine();
}

void Movement::reportCalib(MovementAxis *axis, int calibStatus)
{
  Serial.print(COMM_REPORT_CALIB_STATUS);
  Serial.print(" ");
  Serial.print(axis->channelLabel);
  Serial.print(calibStatus);
  CurrentState::getInstance()->printQAndNewLine();
}

void Movement::checkAxisSubStatus(MovementAxis *axis, int *axisSubStatus)
{
  int newStatus = 0;
  bool statusChanged = false;

  if (axis->isAccelerating())
  {
    newStatus = COMM_REPORT_MOVE_STATUS_ACCELERATING;
  }

  if (axis->isCruising())
  {
    newStatus = COMM_REPORT_MOVE_STATUS_CRUISING;
  }

  if (axis->isDecelerating())
  {
    newStatus = COMM_REPORT_MOVE_STATUS_DECELERATING;
  }

  if (axis->isCrawling())
  {
    newStatus = COMM_REPORT_MOVE_STATUS_CRAWLING;
  }

  // if the status changes, send out a status report
  if (*axisSubStatus != newStatus && newStatus > 0)
  {
    statusChanged = true;
  }
  *axisSubStatus = newStatus;

  if (statusChanged)
  {
    reportStatus(axis, *axisSubStatus);
  }
}

//const int MOVEMENT_INTERRUPT_SPEED = 100; // Interrupt cycle in micro seconds

Movement::Movement()
{

  // Initialize some variables for testing

  motorMotorsEnabled = false;

  motorConsMissedSteps[0] = 0;
  motorConsMissedSteps[1] = 0;
  motorConsMissedSteps[2] = 0;

  motorLastPosition[0] = 0;
  motorLastPosition[1] = 0;
  motorLastPosition[2] = 0;

  motorConsEncoderLastPosition[0] = 0;
  motorConsEncoderLastPosition[1] = 0;
  motorConsEncoderLastPosition[2] = 0;

  // Create the axis controllers

  axisX = MovementAxis();
  axisY = MovementAxis();
  axisZ = MovementAxis();

  axisX.channelLabel = 'X';
  axisY.channelLabel = 'Y';
  axisZ.channelLabel = 'Z';

  // Create the encoder controller

  encoderX = MovementEncoder();
  encoderY = MovementEncoder();
  encoderZ = MovementEncoder();

  // Load settings

  loadSettings();

  motorMotorsEnabled = false;
}

void Movement::loadSettings()
{

  /**/ //Serial.println("== load pin numbers ==");

  // Load motor settings

  axisX.loadPinNumbers(X_STEP_PIN, X_DIR_PIN, X_ENABLE_PIN, X_MIN_PIN, X_MAX_PIN, E_STEP_PIN, E_DIR_PIN, E_ENABLE_PIN);
  axisY.loadPinNumbers(Y_STEP_PIN, Y_DIR_PIN, Y_ENABLE_PIN, Y_MIN_PIN, Y_MAX_PIN, 0, 0, 0);
  axisZ.loadPinNumbers(Z_STEP_PIN, Z_DIR_PIN, Z_ENABLE_PIN, Z_MIN_PIN, Z_MAX_PIN, 0, 0, 0);

  axisSubStep[0] = COMM_REPORT_MOVE_STATUS_IDLE;
  axisSubStep[1] = COMM_REPORT_MOVE_STATUS_IDLE;
  axisSubStep[2] = COMM_REPORT_MOVE_STATUS_IDLE;

  /**/ //Serial.println("== load motor settings ==");

  loadMotorSettings();

  // Load encoder settings

  /**/ //Serial.println("== load encoder settings ==");

  loadEncoderSettings();

  /**/ //Serial.println("== load mdl encoder settings ==");

  encoderX.loadMdlEncoderId(_MDL_X1);
  encoderY.loadMdlEncoderId(_MDL_Y);
  encoderZ.loadMdlEncoderId(_MDL_Z);

  /**/ //Serial.println("== load encoder pin numbers ==");

  encoderX.loadPinNumbers(X_ENCDR_A, X_ENCDR_B, X_ENCDR_A_Q, X_ENCDR_B_Q);
  encoderY.loadPinNumbers(Y_ENCDR_A, Y_ENCDR_B, Y_ENCDR_A_Q, Y_ENCDR_B_Q);
  encoderZ.loadPinNumbers(Z_ENCDR_A, Z_ENCDR_B, Z_ENCDR_A_Q, Z_ENCDR_B_Q);

  /**/ //Serial.println("== load encoder load settings 2 ==");

  encoderX.loadSettings(motorConsEncoderType[0], motorConsEncoderScaling[0], motorConsEncoderInvert[0]);
  encoderY.loadSettings(motorConsEncoderType[1], motorConsEncoderScaling[1], motorConsEncoderInvert[1]);
  encoderZ.loadSettings(motorConsEncoderType[2], motorConsEncoderScaling[2], motorConsEncoderInvert[2]);

}

#if defined(FARMDUINO_EXP_V20) || defined(FARMDUINO_V30) || defined(RAMPS_V16)
  void Movement::initTMC2130()
  {
    axisX.initTMC2130();
    axisY.initTMC2130();
    axisZ.initTMC2130();
  }

  void Movement::loadSettingsTMC2130()
  {
    loadSettingsTMC2130_X();
    loadSettingsTMC2130_Y();
    loadSettingsTMC2130_Z();

    /**/
    //int motorCurrentX;
    //int stallSensitivityX;
    //int microStepsX;

    //int motorCurrentY;
    //int stallSensitivityY;
    //int microStepsY;

    //int motorCurrentZ;
    //int stallSensitivityZ;
    //int microStepsZ;

    //motorCurrentX = ParameterList::getInstance()->getValue(MOVEMENT_MOTOR_CURRENT_X);
    //stallSensitivityX = ParameterList::getInstance()->getValue(MOVEMENT_STALL_SENSITIVITY_X);
    //microStepsX = ParameterList::getInstance()->getValue(MOVEMENT_MICROSTEPS_X);

    //motorCurrentY = ParameterList::getInstance()->getValue(MOVEMENT_MOTOR_CURRENT_Y);
    //stallSensitivityY = ParameterList::getInstance()->getValue(MOVEMENT_STALL_SENSITIVITY_Y);
    //microStepsY = ParameterList::getInstance()->getValue(MOVEMENT_MICROSTEPS_Y);

    //motorCurrentZ = ParameterList::getInstance()->getValue(MOVEMENT_MOTOR_CURRENT_Z);
    //stallSensitivityZ = ParameterList::getInstance()->getValue(MOVEMENT_STALL_SENSITIVITY_Z);
    //microStepsZ = ParameterList::getInstance()->getValue(MOVEMENT_MICROSTEPS_Z);

    //if (microStepsX <= 0) { microStepsX = 1; }
    //if (microStepsY <= 0) { microStepsY = 1; }
    //if (microStepsZ <= 0) { microStepsZ = 1; }


    //axisX.loadSettingsTMC2130(motorCurrentX, stallSensitivityX, microStepsX);
    //axisY.loadSettingsTMC2130(motorCurrentY, stallSensitivityY, microStepsY);
    //axisZ.loadSettingsTMC2130(motorCurrentZ, stallSensitivityZ, microStepsZ);
  }

  void Movement::loadSettingsTMC2130_X()
  {
    int motorCurrentX;
    int stallSensitivityX;
    int microStepsX;

    motorCurrentX = ParameterList::getInstance()->getValue(MOVEMENT_MOTOR_CURRENT_X);
    stallSensitivityX = ParameterList::getInstance()->getValue(MOVEMENT_STALL_SENSITIVITY_X);
    microStepsX = ParameterList::getInstance()->getValue(MOVEMENT_MICROSTEPS_X);

    if (microStepsX <= 0) { microStepsX = 1; }


    axisX.loadSettingsTMC2130(motorCurrentX, stallSensitivityX, microStepsX);
  }

  void Movement::loadSettingsTMC2130_Y()
  {
    int motorCurrentY;
    int stallSensitivityY;
    int microStepsY;

    motorCurrentY = ParameterList::getInstance()->getValue(MOVEMENT_MOTOR_CURRENT_Y);
    stallSensitivityY = ParameterList::getInstance()->getValue(MOVEMENT_STALL_SENSITIVITY_Y);
    microStepsY = ParameterList::getInstance()->getValue(MOVEMENT_MICROSTEPS_Y);

    if (microStepsY <= 0) { microStepsY = 1; }

    axisY.loadSettingsTMC2130(motorCurrentY, stallSensitivityY, microStepsY);
  }

  void Movement::loadSettingsTMC2130_Z()
  {
    int motorCurrentZ;
    int stallSensitivityZ;
    int microStepsZ;

    motorCurrentZ = ParameterList::getInstance()->getValue(MOVEMENT_MOTOR_CURRENT_Z);
    stallSensitivityZ = ParameterList::getInstance()->getValue(MOVEMENT_STALL_SENSITIVITY_Z);
    microStepsZ = ParameterList::getInstance()->getValue(MOVEMENT_MICROSTEPS_Z);

    if (microStepsZ <= 0) { microStepsZ = 1; }

    axisZ.loadSettingsTMC2130(motorCurrentZ, stallSensitivityZ, microStepsZ);
  }

#endif

  static int missedCount = 0;

void Movement::test()
{
  #if defined(FARMDUINO_EXP_V20) || defined(FARMDUINO_V30) || defined(RAMPS_V16)
  //axisX.enableMotor();
  //axisX.setMotorStep();
  //delayMicroseconds(500);
  //TMC2130X.read_STAT();
  //delayMicroseconds(500);
  //if (axisX.stallDetected()) { testA++; }
  //testB++;
      //Serial.print("R99");
      //Serial.print(" running test ");
      //Serial.print("\r\n");
  bool stallGuard = false;
  bool standStill = false;
  uint8_t status_z = 0;

  //digitalWrite(X_ENABLE_PIN, LOW);
  axisZ.enableMotor();
  //digitalWrite(X_STEP_PIN, HIGH);
  axisZ.setMotorStep();

  delayMicroseconds(500);

  //digitalWrite(X_STEP_PIN, LOW);
  axisZ.resetMotorStep();

  TMC2130Z.read_STAT();

  status_z = TMC2130Z.getStatus();
  stallGuard = status_z & FB_TMC_SPISTATUS_STALLGUARD_MASK ? true : false;
  standStill = status_z & FB_TMC_SPISTATUS_STANDSTILL_MASK ? true : false;
  if (stallGuard || standStill) {
    testA++;
  }

  //if (axisX.stallDetected()) {
  //  testA++;
  //}


  testB++;

  delayMicroseconds(500);



  #endif
}

void Movement::test2()
{
  Serial.print("*");
  Serial.print(testA);
  Serial.print("/");
  Serial.print(testB);
  Serial.println();

  //testA = 0;
}

/**
 * xDest - destination X in steps
 * yDest - destination Y in steps
 * zDest - destination Z in steps
 * maxStepsPerSecond - maximum number of steps per second
 * maxAccelerationStepsPerSecond - maximum number of acceleration in steps per second
 */
int Movement::moveToCoords(double xDestScaled, double yDestScaled, double zDestScaled,
                                 unsigned int xMaxSpd, unsigned int yMaxSpd, unsigned int zMaxSpd,
                                 bool xHome, bool yHome, bool zHome)
{

  /**/ //Serial.println("AAA");
  /**/ //test();

  long xDest = xDestScaled * stepsPerMm[0];
  long yDest = yDestScaled * stepsPerMm[1];
  long zDest = zDestScaled * stepsPerMm[2];

  unsigned long currentMillis = 0;
  unsigned long timeStart = millis();

  int highestX = 0;
  int highestY = 0;
  int highestZ = 0;

  serialMessageNr = 0;
  serialMessageDelay = 0;

  int incomingByte = 0;
  int error = 0;
  bool emergencyStop = false;
  unsigned int commandSpeed[3];


  #if defined(FARMDUINO_EXP_V20)|| defined(RAMPS_V16)
  axisX.missedStepHistory[0] = 0;
  axisX.missedStepHistory[1] = 0;
  axisX.missedStepHistory[2] = 0;
  axisX.missedStepHistory[3] = 0;
  axisX.missedStepHistory[4] = 0;

  axisY.missedStepHistory[0] = 0;
  axisY.missedStepHistory[1] = 0;
  axisY.missedStepHistory[2] = 0;
  axisY.missedStepHistory[3] = 0;
  axisY.missedStepHistory[4] = 0;

  axisZ.missedStepHistory[0] = 0;
  axisZ.missedStepHistory[1] = 0;
  axisZ.missedStepHistory[2] = 0;
  axisZ.missedStepHistory[3] = 0;
  axisZ.missedStepHistory[4] = 0;
  #endif

  // if a speed is given in the command, use that instead of the config speed

  if (xMaxSpd > 0 && xMaxSpd < speedMax[0])
  {
    commandSpeed[0] = xMaxSpd;
  }
  else
  {
    commandSpeed[0] = speedMax[0];
  }

  if (yMaxSpd > 0 && yMaxSpd < speedMax[1])
  {
    commandSpeed[1] = yMaxSpd;
  }
    else
  {
    commandSpeed[1] = speedMax[1];
  }

  if (zMaxSpd > 0 && zMaxSpd < speedMax[2])
  {
    commandSpeed[2] = zMaxSpd;
  }
    else
  {
    commandSpeed[2] = speedMax[2];
  }

  axisX.setMaxSpeed(commandSpeed[0]);
  axisY.setMaxSpeed(commandSpeed[1]);
  axisZ.setMaxSpeed(commandSpeed[2]);

  // Load coordinates into axis class

  long sourcePoint[3] = {0, 0, 0};
  sourcePoint[0] = CurrentState::getInstance()->getX();
  sourcePoint[1] = CurrentState::getInstance()->getY();
  sourcePoint[2] = CurrentState::getInstance()->getZ();

  long currentPoint[3] = {0, 0, 0};
  currentPoint[0] = CurrentState::getInstance()->getX();
  currentPoint[1] = CurrentState::getInstance()->getY();
  currentPoint[2] = CurrentState::getInstance()->getZ();

  long destinationPoint[3] = {0, 0, 0};
  destinationPoint[0] = xDest;
  destinationPoint[1] = yDest;
  destinationPoint[2] = zDest;

  long homeMissedSteps[3] = { 0, 0, 0 };

  motorConsMissedSteps[0] = 0;
  motorConsMissedSteps[1] = 0;
  motorConsMissedSteps[2] = 0;

  motorConsMissedStepsPrev[0] = 0;
  motorConsMissedStepsPrev[1] = 0;
  motorConsMissedStepsPrev[2] = 0;

  motorLastPosition[0] = currentPoint[0];
  motorLastPosition[1] = currentPoint[1];
  motorLastPosition[2] = currentPoint[2];

  // Load coordinates into motor control
  // Report back coordinates if target coordinates changed

  if (axisX.loadCoordinates(currentPoint[0], destinationPoint[0], xHome))
  {
    Serial.print(COMM_REPORT_COORD_CHANGED_X);
    Serial.print(" X");
    Serial.print(axisX.destinationPosition() / stepsPerMm[0]);
    CurrentState::getInstance()->printQAndNewLine();
  }

  if (axisY.loadCoordinates(currentPoint[1], destinationPoint[1], yHome))
  {
    Serial.print(COMM_REPORT_COORD_CHANGED_Y);
    Serial.print(" Y");
    Serial.print(axisY.destinationPosition() / stepsPerMm[1]);
    CurrentState::getInstance()->printQAndNewLine();
  }

  if (axisZ.loadCoordinates(currentPoint[2], destinationPoint[2], zHome))
  {
    Serial.print(COMM_REPORT_COORD_CHANGED_Z);
    Serial.print(" Z");
    Serial.print(axisZ.destinationPosition() / stepsPerMm[2]);
    CurrentState::getInstance()->printQAndNewLine();
  }

  // Prepare for movement

  axisX.movementStarted = false;
  axisY.movementStarted = false;
  axisZ.movementStarted = false;

  storeEndStops();
  reportEndStops();

  axisX.setDirectionAxis();
  axisY.setDirectionAxis();
  axisZ.setDirectionAxis();

  // Enable motors

  axisSubStep[0] = COMM_REPORT_MOVE_STATUS_START_MOTOR;
  axisSubStep[1] = COMM_REPORT_MOVE_STATUS_START_MOTOR;
  axisSubStep[2] = COMM_REPORT_MOVE_STATUS_START_MOTOR;

  reportStatus(&axisX, axisSubStep[0]);
  reportStatus(&axisY, axisSubStep[1]);
  reportStatus(&axisZ, axisSubStep[2]);

  enableMotors();

  // Start movement

  axisActive[0] = true;
  axisActive[1] = true;
  axisActive[2] = true;

  if (xHome || yHome || zHome)
  {
    if (!xHome) { axisX.deactivateAxis(); }
    if (!yHome) { axisY.deactivateAxis(); }
    if (!zHome) { axisZ.deactivateAxis(); }

    axisActive[0] = xHome;
    axisActive[1] = yHome;
    axisActive[2] = zHome;
  }

  axisX.resetNrOfSteps();
  axisY.resetNrOfSteps();
  axisZ.resetNrOfSteps();

  axisX.checkMovement();
  axisY.checkMovement();
  axisZ.checkMovement();

  axisX.setTicks();
  axisY.setTicks();
  axisZ.setTicks();

  emergencyStop = CurrentState::getInstance()->isEmergencyStop();

  unsigned long loopCounts = 0;

  // Let the interrupt handle all the movements
  while ((axisActive[0] || axisActive[1] || axisActive[2]) && !emergencyStop)
  {
    #if defined(FARMDUINO_V14) || defined(FARMDUINO_V30)
    checkEncoders();
    #endif

    /**/
    //if (loopCounts % 1000 == 0)
    //{
    //  Serial.print("R99");
    //  Serial.print(" missed step ");
    //  Serial.print(motorConsMissedSteps[1]);
    //  Serial.print(" axis pos ");
    //  Serial.print(axisY.currentPosition());
    //  Serial.print("\r\n");

    //  Serial.print("X - ");
    //  axisX.test();

    //  Serial.print("Y - ");
    //  axisY.test();
    //}
    //loopCounts++;    

    checkAxisSubStatus(&axisX, &axisSubStep[0]);
    checkAxisSubStatus(&axisY, &axisSubStep[1]);
    checkAxisSubStatus(&axisZ, &axisSubStep[2]);

    axisX.checkTiming();
    axisY.checkTiming();
    axisZ.checkTiming();

    if (axisX.isStepDone())
    {
      axisX.checkMovement();
      checkAxisVsEncoder(&axisX, &encoderX, &motorConsMissedSteps[0], &motorLastPosition[0], &motorConsEncoderLastPosition[0], &motorConsEncoderUseForPos[0], &motorConsMissedStepsDecay[0], &motorConsEncoderEnabled[0]);
      axisX.resetStepDone();

      // While homing and being at the place where home is supposed to be
      // start counting how many steps are taken before the motor is deactivated
      if (
          xHome == true &&
          (
            (homeIsUp[0] == false && currentPoint[0] <= 0) ||
            (homeIsUp[0] == true  && currentPoint[0] >= 0)
          )
        )
      {
        homeMissedSteps[0]++;
      }
    }

    if (axisY.isStepDone())
    {
      axisY.checkMovement();
      checkAxisVsEncoder(&axisY, &encoderY, &motorConsMissedSteps[1], &motorLastPosition[1], &motorConsEncoderLastPosition[1], &motorConsEncoderUseForPos[1], &motorConsMissedStepsDecay[1], &motorConsEncoderEnabled[1]);
      axisY.resetStepDone();

      // While homing and being at the place where home is supposed to be
      // start counting how many steps are taken before the motor is deactivated
      if (
          yHome == true &&
          (
            (homeIsUp[1] == false && currentPoint[1] <= 0) ||
            (homeIsUp[1] == true  && currentPoint[1] >= 0)
          )
        )
      {
        homeMissedSteps[1]++;
      }
    }

    if (axisZ.isStepDone())
    {
      axisZ.checkMovement();
      checkAxisVsEncoder(&axisZ, &encoderZ, &motorConsMissedSteps[2], &motorLastPosition[2], &motorConsEncoderLastPosition[2], &motorConsEncoderUseForPos[2], &motorConsMissedStepsDecay[2], &motorConsEncoderEnabled[2]);
      axisZ.resetStepDone();

      // While homing and being at the place where home is supposed to be
      // start counting how many steps are taken before the motor is deactivated
      if (
            zHome == true &&
            (
              (homeIsUp[2] == false && currentPoint[2] <= 0) ||
              (homeIsUp[2] == true  && currentPoint[2] >= 0)
            )
        )
      {
        homeMissedSteps[2]++;
      }

    }

    #if defined(FARMDUINO_EXP_V20)|| defined(RAMPS_V16)
    if 
    (
      axisX.isAxisActive()
      && axisX.missedStepHistory[0] >= motorConsMissedStepsMax[0]
      //&& axisX.missedStepHistory[1] >= motorConsMissedStepsMax[0]
      //&& axisX.missedStepHistory[2] >= motorConsMissedStepsMax[0]
      //&& axisX.missedStepHistory[3] >= motorConsMissedStepsMax[0]
      //&& axisX.missedStepHistory[4] >= motorConsMissedStepsMax[0] 
    )
    #else
    if (axisX.isAxisActive() && motorConsMissedSteps[0] >= motorConsMissedStepsMax[0])
    #endif
    {
      axisX.deactivateAxis();

      serialBuffer += "R99";
      serialBuffer += " deactivate motor X due to ";
      #if defined(FARMDUINO_EXP_V20)|| defined(RAMPS_V16)
      if (axisX.isDriverError())
      {
        serialBuffer += "driver error";
      }
      else
      #endif
      {

        serialBuffer += "missed steps";
      }
      serialBuffer += "\r\n";

      if (xHome)
      {
        encoderX.setPosition(0);
        axisX.setCurrentPosition(0);
      }
      else
      {
        error = ERR_STALL_DETECTED_X;
      }
    }

    #if defined(FARMDUINO_EXP_V20)|| defined(RAMPS_V16)
    if 
    (
      axisY.isAxisActive()
      && axisY.missedStepHistory[0] >= motorConsMissedStepsMax[1] 
      //&& axisY.missedStepHistory[1] >= motorConsMissedStepsMax[1]
      //&& axisY.missedStepHistory[2] >= motorConsMissedStepsMax[1]
      //&& axisY.missedStepHistory[3] >= motorConsMissedStepsMax[1]
      //&& axisY.missedStepHistory[4] >= motorConsMissedStepsMax[1]
    )
    #else
    if (axisY.isAxisActive() && motorConsMissedSteps[1] >= motorConsMissedStepsMax[1])
    #endif
    {
      axisY.deactivateAxis();

      serialBuffer += "R99";
      serialBuffer += " deactivate motor Y due to ";

      #if defined(FARMDUINO_EXP_V20)|| defined(RAMPS_V16)
      if (axisY.isDriverError()) 
      {
        serialBuffer += "driver error";
      }
      else
      #endif
      {

        serialBuffer += "missed steps";
      }
      serialBuffer += "\r\n";

      //serialBuffer += "R99";
      //serialBuffer += " S";
      //serialBuffer += axisY.getNrOfSteps();
      //serialBuffer += " MS ";
      //serialBuffer += motorConsMissedSteps[1];
      //serialBuffer += " MSM ";
      //serialBuffer += motorConsMissedStepsMax[1];
      //serialBuffer += "\r\n";

      if (yHome)
      {
        encoderY.setPosition(0);
        axisY.setCurrentPosition(0);
      }
      else
      {
        error = ERR_STALL_DETECTED_Y;
      }
    }

    #if defined(FARMDUINO_EXP_V20)|| defined(RAMPS_V16)
    if 
    (
      axisZ.isAxisActive()
      && axisZ.missedStepHistory[0] >= motorConsMissedStepsMax[2]
      //&& axisZ.missedStepHistory[1] >= motorConsMissedStepsMax[2]
      //&& axisZ.missedStepHistory[2] >= motorConsMissedStepsMax[2]
      //&& axisZ.missedStepHistory[3] >= motorConsMissedStepsMax[2]
      //&& axisZ.missedStepHistory[4] >= motorConsMissedStepsMax[2]
    )
    #else
    if (axisZ.isAxisActive() && motorConsMissedSteps[2] >= motorConsMissedStepsMax[2])
    #endif
    {
      axisZ.deactivateAxis();

      serialBuffer += "R99";
      serialBuffer += " deactivate motor Z due to ";

      #if defined(FARMDUINO_EXP_V20)|| defined(RAMPS_V16)
      if (axisZ.isDriverError())
      {
        serialBuffer += "driver error";
      }
      else
      #endif
      {

        serialBuffer += "missed steps";
      }
      serialBuffer += "\r\n";

      if (zHome)
      {
        encoderZ.setPosition(0);
        axisZ.setCurrentPosition(0);        
      }
      else
      {
        error = ERR_STALL_DETECTED_Z;
      }
    }

    if (axisX.endStopAxisReached(false))
    {
      axisX.setCurrentPosition(0);
      encoderX.setPosition(0);
    }

    if (axisY.endStopAxisReached(false))
    {
      axisY.setCurrentPosition(0);
      encoderY.setPosition(0);
    }

    if (axisZ.endStopAxisReached(false))
    {
      axisZ.setCurrentPosition(0);
      encoderZ.setPosition(0);
    }

    axisActive[0] = axisX.isAxisActive();
    axisActive[1] = axisY.isAxisActive();
    axisActive[2] = axisZ.isAxisActive();

    currentPoint[0] = axisX.currentPosition();
    currentPoint[1] = axisY.currentPosition();
    currentPoint[2] = axisZ.currentPosition();

    CurrentState::getInstance()->setX(currentPoint[0]);
    CurrentState::getInstance()->setY(currentPoint[1]);
    CurrentState::getInstance()->setZ(currentPoint[2]);

    storeEndStops();

    // Check timeouts
    if (axisActive[0] == true && ((millis() >= timeStart && millis() - timeStart > timeOut[0] * 1000) || (millis() < timeStart && millis() > timeOut[0] * 1000)))
    {
      serialBuffer += COMM_REPORT_TIMEOUT_X;
      serialBuffer += "\r\n";
      serialBuffer += "R99 timeout X axis\r\n";
      error = ERR_TIMEOUT;
    }
    if (axisActive[1] == true && ((millis() >= timeStart && millis() - timeStart > timeOut[1] * 1000) || (millis() < timeStart && millis() > timeOut[1] * 1000)))
    {
      serialBuffer += COMM_REPORT_TIMEOUT_Y;
      serialBuffer += "\r\n";
      serialBuffer += "R99 timeout Y axis\r\n";
      error = ERR_TIMEOUT;
    }
    if (axisActive[2] == true && ((millis() >= timeStart && millis() - timeStart > timeOut[2] * 1000) || (millis() < timeStart && millis() > timeOut[2] * 1000)))
    {
      serialBuffer += COMM_REPORT_TIMEOUT_Z;
      serialBuffer += "\r\n";
      serialBuffer += "R99 timeout Z axis\r\n";
      error = ERR_TIMEOUT;
    }

    // Check if there is an emergency stop command
    if (Serial.available() > 0)
    {
      incomingByte = Serial.read();
      if (incomingByte == 69 || incomingByte == 101)
      {
        serialBuffer += "R99 emergency stop\r\n";

        Serial.print(COMM_REPORT_EMERGENCY_STOP);
        CurrentState::getInstance()->printQAndNewLine();

        emergencyStop = true;

        axisX.deactivateAxis();
        axisY.deactivateAxis();
        axisZ.deactivateAxis();

        axisActive[0] = false;
        axisActive[1] = false;
        axisActive[2] = false;

        error = ERR_EMERGENCY_STOP;
      }
    }

    if (error != 0)
    {
      serialBuffer += "R99 error\r\n";

      axisActive[0] = false;
      axisActive[1] = false;
      axisActive[2] = false;
    }

    // Send the serial buffer one character per cycle to keep motor timing more accuracte
    serialBufferSendNext();

    // Periodically send message still active
    currentMillis++;
    serialMessageDelay++;

    if (serialMessageDelay > 300 && serialBuffer.length() == 0 && serialBufferSending == 0)
    {
      //Serial.print("Y-");
      //axisY.test();/**/

      serialMessageDelay = 0;

      switch(serialMessageNr)
      {
        case 0:
          serialBuffer += COMM_REPORT_CMD_BUSY;
          serialBuffer += CurrentState::getInstance()->getQAndNewLine();
          break;
        case 1:
          serialBuffer += CurrentState::getInstance()->getPosition();
          serialBuffer += CurrentState::getInstance()->getQAndNewLine();
          #if defined(FARMDUINO_V14) || defined(FARMDUINO_V30)
            getEncoderReport();
          #endif
          break;

        case 2:
          //#if defined(FARMDUINO_EXP_V20) || defined(FARMDUINO_V30)          
          //serialBuffer += "R89";
          //serialBuffer += " X";
          //serialBuffer += axisX.getLoad();
          //serialBuffer += " Y";
          //serialBuffer += axisY.getLoad();
          //serialBuffer += " Z";
          //serialBuffer += axisZ.getLoad();
          //serialBuffer += CurrentState::getInstance()->getQAndNewLine();          
          //#endif
          #if defined(FARMDUINO_EXP_V20)          

          highestX = 0;
          highestY = 0;
          highestZ = 0;

          if (axisX.missedStepHistory[0] > highestX) { highestX = axisX.missedStepHistory[0]; }
          if (axisX.missedStepHistory[1] > highestX) { highestX = axisX.missedStepHistory[1]; }
          if (axisX.missedStepHistory[2] > highestX) { highestX = axisX.missedStepHistory[2]; }
          if (axisX.missedStepHistory[3] > highestX) { highestX = axisX.missedStepHistory[3]; }
          if (axisX.missedStepHistory[4] > highestX) { highestX = axisX.missedStepHistory[4]; }

          if (axisY.missedStepHistory[0] > highestY) { highestY = axisY.missedStepHistory[0]; }
          if (axisY.missedStepHistory[1] > highestY) { highestY = axisY.missedStepHistory[1]; }
          if (axisY.missedStepHistory[2] > highestY) { highestY = axisY.missedStepHistory[2]; }
          if (axisY.missedStepHistory[3] > highestY) { highestY = axisY.missedStepHistory[3]; }
          if (axisY.missedStepHistory[4] > highestY) { highestY = axisY.missedStepHistory[4]; }

          if (axisZ.missedStepHistory[0] > highestZ) { highestZ = axisZ.missedStepHistory[0]; }
          if (axisZ.missedStepHistory[1] > highestZ) { highestZ = axisZ.missedStepHistory[1]; }
          if (axisZ.missedStepHistory[2] > highestZ) { highestZ = axisZ.missedStepHistory[2]; }
          if (axisZ.missedStepHistory[3] > highestZ) { highestZ = axisZ.missedStepHistory[3]; }
          if (axisZ.missedStepHistory[4] > highestZ) { highestZ = axisZ.missedStepHistory[4]; }


          serialBuffer += "R89";
          serialBuffer += " U";
          serialBuffer += axisX.getNrOfSteps();
          serialBuffer += " X";
          serialBuffer += highestX;
          serialBuffer += " V";
          serialBuffer += (int)(axisY.getNrOfSteps() / 100) * 100 ;
          serialBuffer += " Y";
          serialBuffer += highestY;
          serialBuffer += " W";
          serialBuffer += axisZ.getNrOfSteps();
          serialBuffer += " Z";
          serialBuffer += highestZ;
          serialBuffer += CurrentState::getInstance()->getQAndNewLine();          


          #endif
          break;

        default:
          break;
      }

      serialMessageNr++;

      #if !defined(FARMDUINO_EXP_V20)|| defined(RAMPS_V16)
      if (serialMessageNr > 1)
      {
        serialMessageNr = 0;
      }
      #endif

      #if defined(FARMDUINO_EXP_V20)|| defined(RAMPS_V16)
      
      if (serialMessageNr > 2)
      {
        serialMessageNr = 0;
      }
      #endif

      serialBufferSending = 0;
      
      if (debugMessages /*&& debugInterrupt*/)
      {

				Serial.print("R99");
				Serial.print(" missed step ");
				Serial.print(motorConsMissedSteps[1]);
				Serial.print(" encoder pos ");
				Serial.print(encoderY.currentPosition());
				Serial.print(" axis pos ");
				Serial.print(axisY.currentPosition());
				Serial.print("\r\n");
      }
    }
  }

  serialBufferEmpty();
  Serial.print("R99 stopped\r\n");

  // Send feedback for homing

  if (xHome && !error && !emergencyStop)
  {
    Serial.print(COMM_REPORT_HOMED_X);
    CurrentState::getInstance()->printQAndNewLine();
  }

  if (yHome && !error && !emergencyStop)
  {
    Serial.print(COMM_REPORT_HOMED_Y);
    CurrentState::getInstance()->printQAndNewLine();
  }

  if (zHome && !error && !emergencyStop)
  {
    Serial.print(COMM_REPORT_HOMED_Z);
    CurrentState::getInstance()->printQAndNewLine();
  }

  // Stop motors

  axisSubStep[0] = COMM_REPORT_MOVE_STATUS_STOP_MOTOR;
  axisSubStep[1] = COMM_REPORT_MOVE_STATUS_STOP_MOTOR;
  axisSubStep[2] = COMM_REPORT_MOVE_STATUS_STOP_MOTOR;

  reportStatus(&axisX, axisSubStep[0]);
  reportStatus(&axisY, axisSubStep[1]);
  reportStatus(&axisZ, axisSubStep[2]);
  serialBufferEmpty();

  disableMotors();

  // Report end statuses

  currentPoint[0] = axisX.currentPosition();
  currentPoint[1] = axisY.currentPosition();
  currentPoint[2] = axisZ.currentPosition();

  CurrentState::getInstance()->setX(currentPoint[0]);
  CurrentState::getInstance()->setY(currentPoint[1]);
  CurrentState::getInstance()->setZ(currentPoint[2]);

  CurrentState::getInstance()->setHomeMissedStepsX(homeMissedSteps[0]);
  CurrentState::getInstance()->setHomeMissedStepsY(homeMissedSteps[1]);
  CurrentState::getInstance()->setHomeMissedStepsZ(homeMissedSteps[2]);

  storeEndStops();
  reportEndStops();
  reportPosition();
  reportEncoders();

  // Report axis idle

  axisSubStep[0] = COMM_REPORT_MOVE_STATUS_IDLE;
  axisSubStep[1] = COMM_REPORT_MOVE_STATUS_IDLE;
  axisSubStep[2] = COMM_REPORT_MOVE_STATUS_IDLE;

  reportStatus(&axisX, axisSubStep[0]);
  reportStatus(&axisY, axisSubStep[1]);
  reportStatus(&axisZ, axisSubStep[2]);
  serialBufferEmpty();

  disableMotors();

  #if defined(FARMDUINO_EXP_V20)|| defined(RAMPS_V16)
  if (axisX.isDriverError())
  {
    Serial.print("R99 reset motor X\r\n");
    delay(100);
    axisX.initTMC2130();
    loadSettingsTMC2130_X();
    delay(100);
  }

  if (axisY.isDriverError())
  {
    Serial.print("R99 reset motor Y\r\n");
    delay(100);
    axisY.initTMC2130();
    loadSettingsTMC2130_Y();
    delay(100);
  }

  if (axisZ.isDriverError())
  {
    Serial.print("R99 reset motor Z\r\n");
    delay(100);
    axisZ.initTMC2130();
    loadSettingsTMC2130_Z();
    delay(100);
  }

  // Report back the final missed steps
  highestX = 0;
  highestY = 0;
  highestZ = 0;

  if (axisX.missedStepHistory[0] > highestX) { highestX = axisX.missedStepHistory[0]; }
  if (axisX.missedStepHistory[1] > highestX) { highestX = axisX.missedStepHistory[1]; }
  if (axisX.missedStepHistory[2] > highestX) { highestX = axisX.missedStepHistory[2]; }
  if (axisX.missedStepHistory[3] > highestX) { highestX = axisX.missedStepHistory[3]; }
  if (axisX.missedStepHistory[4] > highestX) { highestX = axisX.missedStepHistory[4]; }

  if (axisY.missedStepHistory[0] > highestY) { highestY = axisY.missedStepHistory[0]; }
  if (axisY.missedStepHistory[1] > highestY) { highestY = axisY.missedStepHistory[1]; }
  if (axisY.missedStepHistory[2] > highestY) { highestY = axisY.missedStepHistory[2]; }
  if (axisY.missedStepHistory[3] > highestY) { highestY = axisY.missedStepHistory[3]; }
  if (axisY.missedStepHistory[4] > highestY) { highestY = axisY.missedStepHistory[4]; }

  if (axisZ.missedStepHistory[0] > highestZ) { highestZ = axisZ.missedStepHistory[0]; }
  if (axisZ.missedStepHistory[1] > highestZ) { highestZ = axisZ.missedStepHistory[1]; }
  if (axisZ.missedStepHistory[2] > highestZ) { highestZ = axisZ.missedStepHistory[2]; }
  if (axisZ.missedStepHistory[3] > highestZ) { highestZ = axisZ.missedStepHistory[3]; }
  if (axisZ.missedStepHistory[4] > highestZ) { highestZ = axisZ.missedStepHistory[4]; }


  Serial.print("R89");
  Serial.print(" U");
  Serial.print(axisX.getNrOfSteps());
  Serial.print(" X");
  Serial.print(highestX);
  Serial.print(" V");
  Serial.print((int)(axisY.getNrOfSteps() / 100) * 100);
  Serial.print(" Y");
  Serial.print(highestY);
  Serial.print(" W");
  Serial.print(axisZ.getNrOfSteps());
  Serial.print(" Z");
  Serial.print(highestZ);
  Serial.print(CurrentState::getInstance()->getQAndNewLine());


  #endif

  if (emergencyStop)
  {
    CurrentState::getInstance()->setEmergencyStop();
    error = ERR_EMERGENCY_STOP;
  }

  Serial.print("R99 error ");
  Serial.print(error);
  Serial.print("\r\n");


  // Return error
  CurrentState::getInstance()->setLastError(error);

  return error;
}

void Movement::serialBufferEmpty()
{
  while (serialBuffer.length() > 0)
  {
    serialBufferSendNext();
  }
}

void Movement::serialBufferSendNext()
{
  // Send the next char in the serialBuffer
  if (serialBuffer.length() > 0)
  {

    if (serialBufferSending < serialBuffer.length())
    {
      //Serial.print("-");
      switch (serialBuffer.charAt(serialBufferSending))
      {
      case 13:
        Serial.print("\r\n");
        break;
      case 10:
        break;
      default:
        Serial.print(serialBuffer.charAt(serialBufferSending));
        break;
      }

      serialBufferSending++;
    }
    else
    {
      // Reset length of buffer when done
      serialBuffer = "";
      serialBufferSending = 0;
    }
  }
}

//
// Calibration
//

int Movement::calibrateAxis(int axis)
{

  // Load motor and encoder settings

  loadMotorSettings();
  loadEncoderSettings();

  unsigned long timeStart = millis();

  bool movementDone = false;

  int paramValueInt = 0;
  long stepsCount = 0;
  int incomingByte = 0;
  int error = 0;

  bool invertEndStops = false;
  int parEndInv;
  int parNbrStp;

  float *missedSteps;
  int *missedStepsMax;
  long *lastPosition;
  float *encoderStepDecay;
  bool *encoderEnabled;
  int *axisStatus;
  long *axisStepsPerMm;

  long stepDelay = 0;
  unsigned long tickDelay = 0;
  unsigned long nextTick = 0;
  unsigned long lastTick = 0;

#if defined(FARMDUINO_EXP_V20)|| defined(RAMPS_V16)
  axisX.missedStepHistory[0] = 0;
  axisX.missedStepHistory[1] = 0;
  axisX.missedStepHistory[2] = 0;
  axisX.missedStepHistory[3] = 0;
  axisX.missedStepHistory[4] = 0;

  axisY.missedStepHistory[0] = 0;
  axisY.missedStepHistory[1] = 0;
  axisY.missedStepHistory[2] = 0;
  axisY.missedStepHistory[3] = 0;
  axisY.missedStepHistory[4] = 0;

  axisZ.missedStepHistory[0] = 0;
  axisZ.missedStepHistory[1] = 0;
  axisZ.missedStepHistory[2] = 0;
  axisZ.missedStepHistory[3] = 0;
  axisZ.missedStepHistory[4] = 0;
#endif

  // Prepare for movement
  //tickDelay = (1000.0 * 1000.0 / MOVEMENT_INTERRUPT_SPEED / speedHome[axis] / 2);
  tickDelay = (1000.0 * 1000.0 / MOVEMENT_INTERRUPT_SPEED / speedHome[axis] / 4);
  
  #if defined(FARMDUINO_EXP_V20)|| defined(RAMPS_V16)
  //stepDelay = 1000000 / speedHome[axis] / 4;
  stepDelay = 1000000 / speedHome[axis] / 2;
  #else
  stepDelay = 100000 / speedHome[axis] / 2;
  #endif

  Serial.print("R99");
  Serial.print(" ");

  Serial.print("tick delay");
  Serial.print(" ");
  Serial.print(tickDelay);
  Serial.print(" ");

  Serial.print("MOVEMENT_INTERRUPT_SPEED");
  Serial.print(" ");
  Serial.print(MOVEMENT_INTERRUPT_SPEED);
  Serial.print(" ");

  Serial.print("speedHome[axis]");
  Serial.print(" ");
  Serial.print(speedHome[axis]);
  Serial.print(" ");

  Serial.print("\r\n");

  storeEndStops();
  reportEndStops();

  // Select the right axis
  MovementAxis *calibAxis;
  MovementEncoder *calibEncoder;

  switch (axis)
  {
  case 0:
    calibAxis = &axisX;
    calibEncoder = &encoderX;
    parEndInv = MOVEMENT_INVERT_ENDPOINTS_X;
    parNbrStp = MOVEMENT_AXIS_NR_STEPS_X;
    invertEndStops = endStInv[0];
    missedSteps = &motorConsMissedSteps[0];
    missedStepsMax = &motorConsMissedStepsMax[0];
    lastPosition = &motorLastPosition[0];
    encoderStepDecay = &motorConsMissedStepsDecay[0];
    encoderEnabled = &motorConsEncoderEnabled[0];
    axisStatus = &axisSubStep[0];
    axisStepsPerMm = &stepsPerMm[0];
    break;
  case 1:
    calibAxis = &axisY;
    calibEncoder = &encoderY;
    parEndInv = MOVEMENT_INVERT_ENDPOINTS_Y;
    parNbrStp = MOVEMENT_AXIS_NR_STEPS_Y;
    invertEndStops = endStInv[1];
    missedSteps = &motorConsMissedSteps[1];
    missedStepsMax = &motorConsMissedStepsMax[1];
    lastPosition = &motorLastPosition[1];
    encoderStepDecay = &motorConsMissedStepsDecay[1];
    encoderEnabled = &motorConsEncoderEnabled[1];
    axisStatus = &axisSubStep[1];
    axisStepsPerMm = &stepsPerMm[1];
    break;
  case 2:
    calibAxis = &axisZ;
    calibEncoder = &encoderZ;
    parEndInv = MOVEMENT_INVERT_ENDPOINTS_Z;
    parNbrStp = MOVEMENT_AXIS_NR_STEPS_Z;
    invertEndStops = endStInv[2];
    missedSteps = &motorConsMissedSteps[2];
    missedStepsMax = &motorConsMissedStepsMax[2];
    lastPosition = &motorLastPosition[2];
    encoderStepDecay = &motorConsMissedStepsDecay[2];
    encoderEnabled = &motorConsEncoderEnabled[2];
    axisStatus = &axisSubStep[2];
    axisStepsPerMm = &stepsPerMm[2];
    break;
  default:
    Serial.print("R99 Calibration error: invalid axis selected\r\n");
    error = ERR_CALIBRATION_ERROR;
    CurrentState::getInstance()->setLastError(error);
    return error;
  }

  // Preliminary checks

  if (calibAxis->endStopMin() || calibAxis->endStopMax())
  {
    Serial.print("R99 Calibration error: end stop active before start\r\n");
    error = ERR_CALIBRATION_ERROR;
    CurrentState::getInstance()->setLastError(error);
    return error;
  }

  Serial.print("R99");
  Serial.print(" axis ");
  Serial.print(calibAxis->channelLabel);
  Serial.print(" move to start for calibration");
  Serial.print("\r\n");

  *axisStatus = COMM_REPORT_MOVE_STATUS_START_MOTOR;
  reportStatus(calibAxis, axisStatus[0]);

  // Move towards home
  calibAxis->enableMotor();
  
  //calibAxis->setDirectionHome();
  calibAxis->setDirectionAway();

  calibAxis->setCurrentPosition(calibEncoder->currentPosition());

  stepsCount = 0;
  *missedSteps = 0;
  movementDone = false;

  motorConsMissedSteps[0] = 0;
  motorConsMissedSteps[1] = 0;
  motorConsMissedSteps[2] = 0;

  *axisStatus = COMM_REPORT_MOVE_STATUS_CRAWLING;
  reportStatus(calibAxis, axisStatus[0]);

  reportCalib(calibAxis, COMM_REPORT_CALIBRATE_STATUS_TO_HOME);

  while (!movementDone && error == 0)
  {

    #if defined(FARMDUINO_V14) || defined(FARMDUINO_V30)
      checkEncoders();
    #endif

    checkAxisVsEncoder(calibAxis, calibEncoder, &motorConsMissedSteps[axis], &motorLastPosition[axis], &motorConsEncoderLastPosition[axis], &motorConsEncoderUseForPos[axis], &motorConsMissedStepsDecay[axis], &motorConsEncoderEnabled[axis]);

    // Check timeouts
    if (!movementDone && ((millis() >= timeStart && millis() - timeStart > timeOut[axis] * 1000) || (millis() < timeStart && millis() > timeOut[axis] * 1000)))
    {
      calibAxis->disableMotor();
      switch (axis)
      {
      case 0:
        Serial.print(COMM_REPORT_TIMEOUT_X);
        CurrentState::getInstance()->printQAndNewLine();
        Serial.print("R99 timeout X axis\r\n");
        error = ERR_TIMEOUT;
        CurrentState::getInstance()->setLastError(error);
        return error;
      case 1:
        Serial.print(COMM_REPORT_TIMEOUT_Y);
        CurrentState::getInstance()->printQAndNewLine();
        Serial.print("R99 timeout Y axis\r\n");
        error = ERR_TIMEOUT;
        CurrentState::getInstance()->setLastError(error);
        return error;
      case 2:
        Serial.print(COMM_REPORT_TIMEOUT_Z);
        CurrentState::getInstance()->printQAndNewLine();
        Serial.print("R99 timeout Z axis\r\n");
        error = ERR_TIMEOUT;
        CurrentState::getInstance()->setLastError(error);
        return error;
      default:
        Serial.print("R99 Calibration error: invalid axis selected\r\n");
        error = ERR_CALIBRATION_ERROR;
        CurrentState::getInstance()->setLastError(error);
        return error;
      }
    }

    // Check if there is an emergency stop command
    if (Serial.available() > 0)
    {
      incomingByte = Serial.read();
      if (incomingByte == 69 || incomingByte == 101)
      {
        Serial.print("R99 emergency stop\r\n");
        movementDone = true;
        CurrentState::getInstance()->setEmergencyStop();
        Serial.print(COMM_REPORT_EMERGENCY_STOP);
        CurrentState::getInstance()->printQAndNewLine();
        error = ERR_EMERGENCY_STOP;
      }
    }

    // Move until any end stop is reached or the motor is skipping. That end should be the far end stop. First, ram the end at high speed.

    /**/
    //if (((!invertEndStops && !calibAxis->endStopMax()) || (invertEndStops && !calibAxis->endStopMin())) && !movementDone && (*missedSteps < *missedStepsMax))
    //if ((!calibAxis->endStopMin() && !calibAxis->endStopMax()) && !movementDone && (*missedSteps < *missedStepsMax))

    
    //Serial.print("R99 missed steps max ");
    //Serial.print(*missedStepsMax);
    //Serial.print(" * ");
    //Serial.print(calibAxis->missedStepHistory[0]);
    //Serial.print(" ");
    //Serial.print(calibAxis->missedStepHistory[1]);
    //Serial.print(" ");
    //Serial.print(calibAxis->missedStepHistory[2]);
    //Serial.print(" ");
    //Serial.print(calibAxis->missedStepHistory[3]);
    //Serial.print(" ");
    //Serial.print(calibAxis->missedStepHistory[4]);
    //Serial.print(" ");
    //Serial.print("\r\n");

#if defined(FARMDUINO_EXP_V20)|| defined(RAMPS_V16)
    if (
        (!calibAxis->endStopMin() && !calibAxis->endStopMax()) && 
        !movementDone 
        && 
        !(
          calibAxis->missedStepHistory[0] >= *missedStepsMax //&&
          //calibAxis->missedStepHistory[1] >= *missedStepsMax &&
          //calibAxis->missedStepHistory[2] >= *missedStepsMax &&
          //calibAxis->missedStepHistory[3] >= *missedStepsMax &&
          //calibAxis->missedStepHistory[4] >= *missedStepsMax
        )
      )
#else
    if ((!calibAxis->endStopMin() && !calibAxis->endStopMax()) && !movementDone && (*missedSteps < *missedStepsMax))
#endif
    {

      calibAxis->setStepAxis();
      delayMicroseconds(stepDelay);

      stepsCount++;
      if (stepsCount % (speedHome[axis] * 3) == 0)
      {
        // Periodically send message still active
        Serial.print(COMM_REPORT_CMD_BUSY);
        CurrentState::getInstance()->printQAndNewLine();
      }

      if (debugMessages)
      {
        if (stepsCount % (speedHome[axis] / 6) == 0 /*|| *missedSteps > 3*/)
        {
          Serial.print("R99");
          Serial.print(" step count ");
          Serial.print(stepsCount);
          Serial.print(" missed steps ");
          Serial.print(*missedSteps);
          Serial.print(" max steps ");
          Serial.print(*missedStepsMax);
          Serial.print(" cur pos mtr ");
          Serial.print(calibAxis->currentPosition());
          Serial.print(" cur pos enc ");
          Serial.print(calibEncoder->currentPosition());
          Serial.print("\r\n");
        }
      }

      calibAxis->resetMotorStep();

      delayMicroseconds(stepDelay);
    }
    else
    {
      movementDone = true;
      Serial.print("R99 movement done\r\n");

      // If end stop for home is active, set the position to zero
      if (calibAxis->endStopMin())
      {
        invertEndStops = true;
      }
    }
  }

  reportCalib(calibAxis, COMM_REPORT_CALIBRATE_STATUS_TO_END);

  Serial.print("R99");
  Serial.print(" axis ");
  Serial.print(calibAxis->channelLabel);
  Serial.print(" at starting point");
  Serial.print("\r\n");

  // Report back the end stop setting

  if (error == 0)
  {
    if (invertEndStops)
    {
      paramValueInt = 1;
    }
    else
    {
      paramValueInt = 0;
    }

    Serial.print("R23");
    Serial.print(" ");
    Serial.print("P");
    Serial.print(parEndInv);
    Serial.print(" ");
    Serial.print("V");
    Serial.print(paramValueInt);
    //Serial.print("\r\n");
    CurrentState::getInstance()->printQAndNewLine();
  }

  // Store the status of the system

  storeEndStops();
  reportEndStops();

  // Move into the other direction now, and measure the number of steps

  Serial.print("R99");
  Serial.print(" axis ");
  Serial.print(calibAxis->channelLabel);
  Serial.print(" calibrating length");
  Serial.print("\r\n");

  timeStart = millis();

  stepsCount = 0;
  movementDone = false;
  *missedSteps = 0;

  /**/
  //calibAxis->setDirectionAway();
  calibAxis->setDirectionHome();

  calibAxis->setCurrentPosition(calibEncoder->currentPosition());

  motorConsMissedSteps[0] = 0;
  motorConsMissedSteps[1] = 0;
  motorConsMissedSteps[2] = 0;

  long encoderStartPoint = calibEncoder->currentPosition();
  long encoderEndPoint = calibEncoder->currentPosition();

  while (!movementDone && error == 0)
  {

    #if defined(FARMDUINO_V14) || defined(FARMDUINO_V30)
       checkEncoders();
    #endif

    checkAxisVsEncoder(calibAxis, calibEncoder, &motorConsMissedSteps[axis], &motorLastPosition[axis], &motorConsEncoderLastPosition[axis], &motorConsEncoderUseForPos[axis], &motorConsMissedStepsDecay[axis], &motorConsEncoderEnabled[axis]);

    // Check timeouts
    if (!movementDone && ((millis() >= timeStart && millis() - timeStart > timeOut[axis] * 1000) || (millis() < timeStart && millis() > timeOut[axis] * 1000)))
    {
      calibAxis->disableMotor();
      switch (axis)
      {
      case 0:
        Serial.print(COMM_REPORT_TIMEOUT_X);
        CurrentState::getInstance()->printQAndNewLine();
        Serial.print("R99 timeout X axis\r\n");
        error = ERR_TIMEOUT;
        CurrentState::getInstance()->setLastError(error);
        return error;
      case 1:
        Serial.print(COMM_REPORT_TIMEOUT_Y);
        CurrentState::getInstance()->printQAndNewLine();
        Serial.print("R99 timeout Y axis\r\n");
        error = ERR_TIMEOUT;
        CurrentState::getInstance()->setLastError(error);
        return error;
      case 2:
        Serial.print(COMM_REPORT_TIMEOUT_Z);
        CurrentState::getInstance()->printQAndNewLine();
        Serial.print("R99 timeout Z axis\r\n");
        error = ERR_TIMEOUT;
        CurrentState::getInstance()->setLastError(error);
        return error;
      default:
        Serial.print("R99 Calibration error: invalid axis selected\r\n");
        error = ERR_CALIBRATION_ERROR;
        CurrentState::getInstance()->setLastError(error);
        return error;
      }
    }

    // Check if there is an emergency stop command
    if (Serial.available() > 0)
    {
      incomingByte = Serial.read();
      if (incomingByte == 69 || incomingByte == 101)
      {
        Serial.print("R99 emergency stop\r\n");
        movementDone = true;
        CurrentState::getInstance()->setEmergencyStop();
        Serial.print(COMM_REPORT_EMERGENCY_STOP);
        CurrentState::getInstance()->printQAndNewLine();
        error = ERR_EMERGENCY_STOP;
      }
    }

    // Ignore the missed steps at startup time
    if (stepsCount < 10)
    {
      *missedSteps = 0;
    }

    // Move until the end stop is at the home position by detecting the other end stop or missed steps are detected
    /**/
    //if ((!calibAxis->endStopMin() && !calibAxis->endStopMax()) && !movementDone && (*missedSteps < *missedStepsMax))
    //if (((!invertEndStops && !calibAxis->endStopMax()) || (invertEndStops && !calibAxis->endStopMin())) && !movementDone && (*missedSteps < *missedStepsMax))
#if defined(FARMDUINO_EXP_V20)|| defined(RAMPS_V16)
    if (
        ((!invertEndStops && !calibAxis->endStopMin()) || (invertEndStops && !calibAxis->endStopMax())) && 
        !movementDone && 
        !(
          calibAxis->missedStepHistory[0] >= *missedStepsMax //&&
          //calibAxis->missedStepHistory[1] >= *missedStepsMax &&
          //calibAxis->missedStepHistory[2] >= *missedStepsMax &&
          //calibAxis->missedStepHistory[3] >= *missedStepsMax &&
          //calibAxis->missedStepHistory[4] >= *missedStepsMax
        )
      )
#else
    if (((!invertEndStops && !calibAxis->endStopMin()) || (invertEndStops && !calibAxis->endStopMax())) && !movementDone && (*missedSteps < *missedStepsMax))
#endif
    {

      calibAxis->setStepAxis();
      stepsCount++;

      //checkAxisVsEncoder(&axisX, &encoderX, &motorConsMissedSteps[0], &motorLastPosition[0], &motorConsMissedStepsDecay[0], &motorConsEncoderEnabled[0]);

      delayMicroseconds(stepDelay);

      if (stepsCount % (speedHome[axis] * 3) == 0)
      {
        // Periodically send message still active
        Serial.print(COMM_REPORT_CMD_BUSY);
        //Serial.print("\r\n");
        CurrentState::getInstance()->printQAndNewLine();

        Serial.print("R99");
        Serial.print(" step count: ");
        Serial.print(stepsCount);
        Serial.print("\r\n");
      }

      calibAxis->resetMotorStep();
      delayMicroseconds(stepDelay);

    }
    else
    {
      Serial.print("R99 movement done\r\n");
      movementDone = true;
    }
  }

  Serial.print("R99");
  Serial.print(" axis ");
  Serial.print(calibAxis->channelLabel);
  Serial.print(" at end point");
  Serial.print("\r\n");

  encoderEndPoint = calibEncoder->currentPosition();

  // if the encoder is enabled, use the encoder data instead of the step count

  if (encoderEnabled)
  {
    stepsCount = abs(encoderEndPoint - encoderStartPoint);
  }

  // Report back the end stop setting

  if (error == 0)
  {
    Serial.print("R23");
    Serial.print(" ");
    Serial.print("P");
    Serial.print(parNbrStp);
    Serial.print(" ");
    Serial.print("V");
    Serial.print((float)stepsCount);
    CurrentState::getInstance()->printQAndNewLine();
  }

  *axisStatus = COMM_REPORT_MOVE_STATUS_STOP_MOTOR;
  reportStatus(calibAxis, axisStatus[0]);

  calibAxis->disableMotor();

  storeEndStops();
  reportEndStops();

  switch (axis)
  {
  case 0:
    CurrentState::getInstance()->setX(stepsCount);
    break;
  case 1:
    CurrentState::getInstance()->setY(stepsCount);
    break;
  case 2:
    CurrentState::getInstance()->setZ(stepsCount);
    break;
  }

  reportPosition();

  *axisStatus = COMM_REPORT_MOVE_STATUS_IDLE;
  reportStatus(calibAxis, axisStatus[0]);

  reportCalib(calibAxis, COMM_REPORT_CALIBRATE_STATUS_IDLE);

  CurrentState::getInstance()->setLastError(error);
  return error;
}

int debugPrintCount = 0;

// Check encoder to verify the motor is at the right position
#if !defined(FARMDUINO_EXP_V20) && !defined(RAMPS_V16)
void Movement::checkAxisVsEncoder(MovementAxis *axis, MovementEncoder *encoder, float *missedSteps, long *lastPosition, long *encoderLastPosition, int *encoderUseForPos, float *encoderStepDecay, bool *encoderEnabled)
{
  if (*encoderEnabled)
  {
    bool stepMissed = false;

    if (debugMessages)
    {
		  //debugPrintCount++;
		  //if (debugPrintCount % 50 == 0)
		  {
			  Serial.print("R99");
			  Serial.print(" encoder pos ");
			  Serial.print(encoder->currentPosition());
        Serial.print(" last enc ");
        Serial.print(*encoderLastPosition);
        Serial.print(" axis pos ");
			  Serial.print(axis->currentPosition());
			  Serial.print(" last pos ");
			  Serial.print(*lastPosition);
			  Serial.print(" move up ");
			  Serial.print(axis->movingUp());
			  Serial.print(" missed step cons ");
			  Serial.print(motorConsMissedSteps[0]);
			  Serial.print(" missed step ");
			  Serial.print(*missedSteps);
			  //Serial.print(" encoder X pos ");
			  //Serial.print(encoderX.currentPosition());
			  //Serial.print(" axis X pos ");
			  //Serial.print(axisX.currentPosition());
			  Serial.print(" decay ");
			  Serial.print(*encoderStepDecay);
			  Serial.print(" enabled ");
			  Serial.print(*encoderEnabled);
			  Serial.print("\r\n");
		  }
		  
    }

    // Decrease amount of missed steps if there are no missed step
    if (*missedSteps > 0)
    {
      (*missedSteps) -= (*encoderStepDecay);
    }
    
    // Check if the encoder goes in the wrong direction or nothing moved
    if ((axis->movingUp() && *encoderLastPosition > encoder->currentPositionRaw()) ||
        (!axis->movingUp() && *encoderLastPosition < encoder->currentPositionRaw()))
    {
      stepMissed = true;
    }

    if (stepMissed && *missedSteps < 32000)
    {
      (*missedSteps)++;
    }

    *encoderLastPosition = encoder->currentPositionRaw();
    *lastPosition = axis->currentPosition();

    //axis->resetStepDone();

    if (*encoderUseForPos)
    {
      axis->setCurrentPosition(encoder->currentPosition());
    }
  }
}
#endif

#if defined(FARMDUINO_EXP_V20)|| defined(RAMPS_V16)
void Movement::checkAxisVsEncoder(MovementAxis *axis, MovementEncoder *encoder, float *missedSteps, long *lastPosition, long *encoderLastPosition, int *encoderUseForPos, float *encoderStepDecay, bool *encoderEnabled)
{
  /**/
  bool stallGuard = false;
  bool standStill = false;
  bool driverError = false;

  uint8_t status = 0;

  axis->readDriverStatus();

  if (*encoderEnabled) {    

    //status = axis->getStatus();
    //stallGuard = status & FB_TMC_SPISTATUS_STALLGUARD_MASK ? true : false;
    //standStill = status & FB_TMC_SPISTATUS_STANDSTILL_MASK ? true : false;
    //driverError = status & FB_TMC_SPISTATUS_ERROR_MASK ? true : false;

    stallGuard = axis->isStallGuard();
    standStill = axis->isStandStill();
    driverError = axis->isDriverError();

    //if (driverError)
    //{
    //  Serial.println("R99 DRIVER ERROR !!**");
    //}


    //if (standStill)
    //{
    //  Serial.println("R99 STANDSTILL !!**");
    //}

    // Reset every hunderd steps, so the missed steps represents the number of missed steps 
    // out of every hundred steps done
    if (axis->getNrOfSteps() % 100 == 0)
    {
      // save the history
      axis->missedStepHistory[4] = axis->missedStepHistory[3];
      axis->missedStepHistory[3] = axis->missedStepHistory[2];
      axis->missedStepHistory[2] = axis->missedStepHistory[1];
      axis->missedStepHistory[1] = axis->missedStepHistory[0];
      axis->missedStepHistory[0] = *missedSteps;

      // test print
      //Serial.print("R99");
      //Serial.print(" ");
      //Serial.print("S = ");
      //Serial.print(" ");
      //Serial.print(axis->getNrOfSteps());
      //Serial.print(" ");
      //Serial.print("M = ");
      //Serial.print(" ");
      //Serial.print(*missedSteps);
      //Serial.print("\r\n");

      // reset missed steps
      *missedSteps = 0;
    }

    //if ((stallGuard || standStill || driverError) && axis->getNrOfSteps() >= *encoderStepDecay) {
    if (stallGuard || standStill || driverError) {
      // In case of stall detection, count this as a missed step. But ignore the first steps

      (*missedSteps)++;
    }

    *lastPosition = axis->currentPosition();
    encoder->setPosition(axis->currentPosition());

  }
}
#endif

void Movement::loadMotorSettings()
{

  // Load settings

  homeIsUp[0] = ParameterList::getInstance()->getValue(MOVEMENT_HOME_UP_X);
  homeIsUp[1] = ParameterList::getInstance()->getValue(MOVEMENT_HOME_UP_Y);
  homeIsUp[2] = ParameterList::getInstance()->getValue(MOVEMENT_HOME_UP_Z);

  speedMax[0] = ParameterList::getInstance()->getValue(MOVEMENT_MAX_SPD_X);
  speedMax[1] = ParameterList::getInstance()->getValue(MOVEMENT_MAX_SPD_Y);
  speedMax[2] = ParameterList::getInstance()->getValue(MOVEMENT_MAX_SPD_Z);

  speedMin[0] = ParameterList::getInstance()->getValue(MOVEMENT_MIN_SPD_X);
  speedMin[1] = ParameterList::getInstance()->getValue(MOVEMENT_MIN_SPD_Y);
  speedMin[2] = ParameterList::getInstance()->getValue(MOVEMENT_MIN_SPD_Z);

  speedHome[0] = ParameterList::getInstance()->getValue(MOVEMENT_HOME_SPEED_X);
  speedHome[1] = ParameterList::getInstance()->getValue(MOVEMENT_HOME_SPEED_Y);
  speedHome[2] = ParameterList::getInstance()->getValue(MOVEMENT_HOME_SPEED_Z);

  stepsAcc[0] = ParameterList::getInstance()->getValue(MOVEMENT_STEPS_ACC_DEC_X);
  stepsAcc[1] = ParameterList::getInstance()->getValue(MOVEMENT_STEPS_ACC_DEC_Y);
  stepsAcc[2] = ParameterList::getInstance()->getValue(MOVEMENT_STEPS_ACC_DEC_Z);

  motorInv[0] = intToBool(ParameterList::getInstance()->getValue(MOVEMENT_INVERT_MOTOR_X));
  motorInv[1] = intToBool(ParameterList::getInstance()->getValue(MOVEMENT_INVERT_MOTOR_Y));
  motorInv[2] = intToBool(ParameterList::getInstance()->getValue(MOVEMENT_INVERT_MOTOR_Z));

  endStInv[0] = ParameterList::getInstance()->getValue(MOVEMENT_INVERT_ENDPOINTS_X);
  endStInv[1] = ParameterList::getInstance()->getValue(MOVEMENT_INVERT_ENDPOINTS_Y);
  endStInv[2] = ParameterList::getInstance()->getValue(MOVEMENT_INVERT_ENDPOINTS_Z);

  endStInv2[0] = ParameterList::getInstance()->getValue(MOVEMENT_INVERT_2_ENDPOINTS_X);
  endStInv2[1] = ParameterList::getInstance()->getValue(MOVEMENT_INVERT_2_ENDPOINTS_Y);
  endStInv2[2] = ParameterList::getInstance()->getValue(MOVEMENT_INVERT_2_ENDPOINTS_Z);

  endStEnbl[0] = intToBool(ParameterList::getInstance()->getValue(MOVEMENT_ENABLE_ENDPOINTS_X));
  endStEnbl[1] = intToBool(ParameterList::getInstance()->getValue(MOVEMENT_ENABLE_ENDPOINTS_Y));
  endStEnbl[2] = intToBool(ParameterList::getInstance()->getValue(MOVEMENT_ENABLE_ENDPOINTS_Z));

  timeOut[0] = ParameterList::getInstance()->getValue(MOVEMENT_TIMEOUT_X);
  timeOut[1] = ParameterList::getInstance()->getValue(MOVEMENT_TIMEOUT_Y);
  timeOut[2] = ParameterList::getInstance()->getValue(MOVEMENT_TIMEOUT_Z);

  motorKeepActive[0] = ParameterList::getInstance()->getValue(MOVEMENT_KEEP_ACTIVE_X);
  motorKeepActive[1] = ParameterList::getInstance()->getValue(MOVEMENT_KEEP_ACTIVE_Y);
  motorKeepActive[2] = ParameterList::getInstance()->getValue(MOVEMENT_KEEP_ACTIVE_Z);

  motorMaxSize[0] = ParameterList::getInstance()->getValue(MOVEMENT_AXIS_NR_STEPS_X);
  motorMaxSize[1] = ParameterList::getInstance()->getValue(MOVEMENT_AXIS_NR_STEPS_Y);
  motorMaxSize[2] = ParameterList::getInstance()->getValue(MOVEMENT_AXIS_NR_STEPS_Z);

  motor2Inv[0] = intToBool(ParameterList::getInstance()->getValue(MOVEMENT_SECONDARY_MOTOR_INVERT_X));
  motor2Inv[1] = false;
  motor2Inv[2] = false;

  motor2Enbl[0] = intToBool(ParameterList::getInstance()->getValue(MOVEMENT_SECONDARY_MOTOR_X));
  motor2Enbl[1] = false;
  motor2Enbl[2] = false;

  motorStopAtHome[0] = intToBool(ParameterList::getInstance()->getValue(MOVEMENT_STOP_AT_HOME_X));
  motorStopAtHome[1] = intToBool(ParameterList::getInstance()->getValue(MOVEMENT_STOP_AT_HOME_Y));
  motorStopAtHome[2] = intToBool(ParameterList::getInstance()->getValue(MOVEMENT_STOP_AT_HOME_Z));

  motorStopAtMax[0] = intToBool(ParameterList::getInstance()->getValue(MOVEMENT_STOP_AT_MAX_X));
  motorStopAtMax[1] = intToBool(ParameterList::getInstance()->getValue(MOVEMENT_STOP_AT_MAX_Y));
  motorStopAtMax[2] = intToBool(ParameterList::getInstance()->getValue(MOVEMENT_STOP_AT_MAX_Z));

  stepsPerMm[0] = ParameterList::getInstance()->getValue(MOVEMENT_STEP_PER_MM_X);
  stepsPerMm[1] = ParameterList::getInstance()->getValue(MOVEMENT_STEP_PER_MM_Y);
  stepsPerMm[2] = ParameterList::getInstance()->getValue(MOVEMENT_STEP_PER_MM_Z);

  if (stepsPerMm[0] < 1) 
  {
    stepsPerMm[0] = 1;
  }

  if (stepsPerMm[1] < 1)
  {
    stepsPerMm[1] = 1;
  }

  if (stepsPerMm[2] < 1)
  {
    stepsPerMm[2] = 1;
  }

  CurrentState::getInstance()->setStepsPerMm(stepsPerMm[0], stepsPerMm[1], stepsPerMm[2]);

  axisX.loadMotorSettings(speedMax[0], speedMin[0], speedHome[0], stepsAcc[0], timeOut[0], homeIsUp[0], motorInv[0], endStInv[0], endStInv2[0], MOVEMENT_INTERRUPT_SPEED, motor2Enbl[0], motor2Inv[0], endStEnbl[0], motorStopAtHome[0], motorMaxSize[0], motorStopAtMax[0]);
  axisY.loadMotorSettings(speedMax[1], speedMin[1], speedHome[1], stepsAcc[1], timeOut[1], homeIsUp[1], motorInv[1], endStInv[1], endStInv2[1], MOVEMENT_INTERRUPT_SPEED, motor2Enbl[1], motor2Inv[1], endStEnbl[1], motorStopAtHome[1], motorMaxSize[1], motorStopAtMax[1]);
  axisZ.loadMotorSettings(speedMax[2], speedMin[2], speedHome[2], stepsAcc[2], timeOut[2], homeIsUp[2], motorInv[2], endStInv[2], endStInv2[2], MOVEMENT_INTERRUPT_SPEED, motor2Enbl[2], motor2Inv[2], endStEnbl[2], motorStopAtHome[2], motorMaxSize[2], motorStopAtMax[2]);

  /**/

/*
#if defined(FARMDUINO_EXP_V20) || defined(FARMDUINO_V30)
  loadSettingsTMC2130();
#endif
*/
  primeMotors();
}

bool Movement::intToBool(int value)
{
  if (value == 1)
  {
    return true;
  }
  return false;
}

void Movement::loadEncoderSettings()
{

  // Load encoder settings

  motorConsMissedStepsMax[0] = ParameterList::getInstance()->getValue(ENCODER_MISSED_STEPS_MAX_X);
  motorConsMissedStepsMax[1] = ParameterList::getInstance()->getValue(ENCODER_MISSED_STEPS_MAX_Y);
  motorConsMissedStepsMax[2] = ParameterList::getInstance()->getValue(ENCODER_MISSED_STEPS_MAX_Z);

  motorConsMissedStepsDecay[0] = ParameterList::getInstance()->getValue(ENCODER_MISSED_STEPS_DECAY_X);
  motorConsMissedStepsDecay[1] = ParameterList::getInstance()->getValue(ENCODER_MISSED_STEPS_DECAY_Y);
  motorConsMissedStepsDecay[2] = ParameterList::getInstance()->getValue(ENCODER_MISSED_STEPS_DECAY_Z);

#if !defined(FARMDUINO_EXP_V20)|| defined(RAMPS_V16)
  motorConsMissedStepsDecay[0] = motorConsMissedStepsDecay[0] / 100;
  motorConsMissedStepsDecay[1] = motorConsMissedStepsDecay[1] / 100;
  motorConsMissedStepsDecay[2] = motorConsMissedStepsDecay[2] / 100;

  motorConsMissedStepsDecay[0] = min(max(motorConsMissedStepsDecay[0], 0.01), 99);
  motorConsMissedStepsDecay[1] = min(max(motorConsMissedStepsDecay[1], 0.01), 99);
  motorConsMissedStepsDecay[2] = min(max(motorConsMissedStepsDecay[2], 0.01), 99);
#endif

  motorConsEncoderType[0] = ParameterList::getInstance()->getValue(ENCODER_TYPE_X);
  motorConsEncoderType[1] = ParameterList::getInstance()->getValue(ENCODER_TYPE_Y);
  motorConsEncoderType[2] = ParameterList::getInstance()->getValue(ENCODER_TYPE_Z);

  motorConsEncoderScaling[0] = ParameterList::getInstance()->getValue(ENCODER_SCALING_X);
  motorConsEncoderScaling[1] = ParameterList::getInstance()->getValue(ENCODER_SCALING_Y);
  motorConsEncoderScaling[2] = ParameterList::getInstance()->getValue(ENCODER_SCALING_Z);

  motorConsEncoderUseForPos[0] = ParameterList::getInstance()->getValue(ENCODER_USE_FOR_POS_X);
  motorConsEncoderUseForPos[1] = ParameterList::getInstance()->getValue(ENCODER_USE_FOR_POS_Y);
  motorConsEncoderUseForPos[2] = ParameterList::getInstance()->getValue(ENCODER_USE_FOR_POS_Z);

  motorConsEncoderInvert[0] = ParameterList::getInstance()->getValue(ENCODER_INVERT_X);
  motorConsEncoderInvert[1] = ParameterList::getInstance()->getValue(ENCODER_INVERT_Y);
  motorConsEncoderInvert[2] = ParameterList::getInstance()->getValue(ENCODER_INVERT_Z);

  if (ParameterList::getInstance()->getValue(ENCODER_ENABLED_X) == 1)
  {
    motorConsEncoderEnabled[0] = true;
  }
  else
  {
    motorConsEncoderEnabled[0] = false;
  }

  if (ParameterList::getInstance()->getValue(ENCODER_ENABLED_Y) == 1)
  {
    motorConsEncoderEnabled[1] = true;
  }
  else
  {
    motorConsEncoderEnabled[1] = false;
  }

  if (ParameterList::getInstance()->getValue(ENCODER_ENABLED_Z) == 1)
  {
    motorConsEncoderEnabled[2] = true;
  }
  else
  {
    motorConsEncoderEnabled[2] = false;
  }
}

unsigned long Movement::getMaxLength(unsigned long lengths[3])
{
  unsigned long max = lengths[0];
  for (int i = 1; i < 3; i++)
  {
    if (lengths[i] > max)
    {
      max = lengths[i];
    }
  }
  return max;
}

void Movement::enableMotors()
{
  motorMotorsEnabled = true;

  axisX.enableMotor();
  axisY.enableMotor();
  axisZ.enableMotor();

  delay(100);
}

void Movement::disableMotorsEmergency()
{
  motorMotorsEnabled = false;

  axisX.disableMotor();
  axisY.disableMotor();
  axisZ.disableMotor();
}

void Movement::disableMotors()
{
  motorMotorsEnabled = false;

  if (motorKeepActive[0] == 0) { axisX.disableMotor(); }
  if (motorKeepActive[1] == 0) { axisY.disableMotor(); }
  if (motorKeepActive[2] == 0) { axisZ.disableMotor(); }

  delay(100);
}

void Movement::primeMotors()
{
  if (motorKeepActive[0] == 1) { axisX.enableMotor(); } else { axisX.disableMotor(); }
  if (motorKeepActive[1] == 1) { axisY.enableMotor(); } else { axisY.disableMotor(); }
  if (motorKeepActive[2] == 1) { axisZ.enableMotor(); } else { axisZ.disableMotor(); }
}

bool Movement::motorsEnabled()
{
  return motorMotorsEnabled;
}

bool Movement::endStopsReached()
{

  if (axisX.endStopsReached() ||
      axisY.endStopsReached() ||
      axisZ.endStopsReached())
  {
    return true;
  }
  return false;
}

void Movement::storePosition()
{

#if !defined(FARMDUINO_EXP_V20)|| defined(RAMPS_V16)
  if (motorConsEncoderEnabled[0])
  {
    CurrentState::getInstance()->setX(encoderX.currentPosition());
  }
  else
  {
    CurrentState::getInstance()->setX(axisX.currentPosition());
  }

  if (motorConsEncoderEnabled[1])
  {
    CurrentState::getInstance()->setY(encoderY.currentPosition());
  }
  else
  {
    CurrentState::getInstance()->setY(axisY.currentPosition());
  }

  if (motorConsEncoderEnabled[2])
  {
    CurrentState::getInstance()->setZ(encoderZ.currentPosition());
  }
  else
  {
    CurrentState::getInstance()->setZ(axisZ.currentPosition());
  }
#endif

#if defined(FARMDUINO_EXP_V20)|| defined(RAMPS_V16)
  CurrentState::getInstance()->setX(axisX.currentPosition());
  CurrentState::getInstance()->setY(axisY.currentPosition());
  CurrentState::getInstance()->setZ(axisZ.currentPosition());
#endif

}

void Movement::reportEndStops()
{
  CurrentState::getInstance()->printEndStops();
}

void Movement::reportPosition()
{
  CurrentState::getInstance()->printPosition();
}

void Movement::storeEndStops()
{
  CurrentState::getInstance()->storeEndStops();
}

void Movement::setPositionX(long pos)
{
  axisX.setCurrentPosition(pos);
  encoderX.setPosition(pos);
}

void Movement::setPositionY(long pos)
{
  axisY.setCurrentPosition(pos);
  encoderY.setPosition(pos);
}

void Movement::setPositionZ(long pos)
{
  axisZ.setCurrentPosition(pos);
  encoderZ.setPosition(pos);
}

// Handle movement by checking each axis
void Movement::handleMovementInterrupt(void)
{
  // No need to check the encoders for Farmduino 1.4
  #if defined(RAMPS_V14) || defined(FARMDUINO_V10)
    checkEncoders();
  #endif

  // handle motor timing
  axisX.incrementTick();
  axisY.incrementTick();
  axisZ.incrementTick();
  calibrationTicks++;

}

void Movement::checkEncoders()
{
  // read encoder pins using the arduino IN registers instead of digital in
  // because it used much fewer cpu cycles

  // A=16/PH1 B=17/PH0 AQ=31/PC6 BQ=33/PC4
  encoderX.checkEncoder(
    ENC_X_A_PORT   & ENC_X_A_BYTE,
    ENC_X_B_PORT   & ENC_X_B_BYTE,
    ENC_X_A_Q_PORT & ENC_X_A_Q_BYTE,
    ENC_X_B_Q_PORT & ENC_X_B_Q_BYTE);

  // A=23/PA1 B=25/PA3 AQ=35/PC2 BQ=37/PC0
  encoderY.checkEncoder(
    ENC_Y_A_PORT   & ENC_Y_A_BYTE,
    ENC_Y_B_PORT   & ENC_Y_B_BYTE,
    ENC_Y_A_Q_PORT & ENC_Y_A_Q_BYTE,
    ENC_Y_B_Q_PORT & ENC_Y_B_Q_BYTE);

  // A=27/PA5 B=29/PA7 AQ=39/PG2 BQ=41/PG0
  encoderZ.checkEncoder(
    ENC_Z_A_PORT   & ENC_Z_A_BYTE,
    ENC_Z_B_PORT   & ENC_Z_B_BYTE,
    ENC_Z_A_Q_PORT & ENC_Z_A_Q_BYTE,
    ENC_Z_B_Q_PORT & ENC_Z_B_Q_BYTE);
}
