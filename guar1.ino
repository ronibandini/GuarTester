// GUAR tester - Based on the PEAR Laboratory experiments
// Roni Bandini, 10/2026, GP License
// UNO R4 minima, 7-segment display, push button and potentiometer

#include <Arduino.h>
#include <TM1637Display.h>
#include <trng.h>
#include <math.h>

// Hardware pins
const uint8_t displayClkPin = 2;
const uint8_t displayDioPin = 3;
const uint8_t buttonPin = 4;
const uint8_t selectorPin = A0;

// Button connected between D4 and GND
const uint8_t buttonPressedState = LOW;
const uint8_t buttonReleasedState = HIGH;
const uint32_t buttonDebounceMs = 40;

// Experiment configuration
const uint16_t bitsPerTrial = 200;
const uint8_t trialsPerSession = 60;

// Timing
const uint16_t trialIndicatorMs = 1000;
const uint16_t selectorPollIntervalMs = 200;
// Configure the reading pause for every result screen (milliseconds).
const uint16_t resultReadPauseMs = 3000;

// Potentiometer range
// Physical left = ADC near 690 = LOW
// Physical right = ADC near 0 = HIGH
const uint16_t selectorMaxRaw = 690;
const uint16_t lowBoundary = 230;
const uint16_t highBoundary = 460;
const uint16_t selectorHysteresis = 12;
const uint8_t selectorStablePolls = 3;

// Statistical threshold
const double zThreshold = 1.645;
const uint8_t displayBrightness = 5;

enum Condition : uint8_t {
  conditionBaseline = 0,
  conditionHigh = 1,
  conditionLow = 2
};

enum Classification : uint8_t {
  classificationNoFlag,
  classificationSuccess,
  classificationFail,
  classificationBaselineFlag
};

struct ActiveStats {
  uint32_t targetBits = 0;
  uint32_t totalBits = 0;
  uint32_t zeros = 0;
  uint32_t ones = 0;
  uint32_t trials = 0;
};

TM1637Display display(displayClkPin, displayDioPin);
ActiveStats activeStats;

uint32_t sessionNumber = 0;
uint32_t accumulationId = 1;

Condition currentCondition = conditionBaseline;
Condition candidateCondition = conditionBaseline;

uint16_t currentSelectorRaw = 0;
uint8_t candidateStableCount = 0;
uint32_t lastSelectorUpdateMs = 0;

// Button state
uint8_t buttonLastReading = HIGH;
uint8_t buttonStableReading = HIGH;
uint32_t buttonLastChangeMs = 0;
bool buttonArmed = false;

// Seven-segment characters
const uint8_t charBlank = 0x00;
const uint8_t charA = SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G;
const uint8_t charB = SEG_C | SEG_D | SEG_E | SEG_F | SEG_G;
const uint8_t charC = SEG_A | SEG_D | SEG_E | SEG_F;
const uint8_t charD = SEG_B | SEG_C | SEG_D | SEG_E | SEG_G;
const uint8_t charE = SEG_A | SEG_D | SEG_E | SEG_F | SEG_G;
const uint8_t charF = SEG_A | SEG_E | SEG_F | SEG_G;
const uint8_t charG = SEG_A | SEG_C | SEG_D | SEG_E | SEG_F;
const uint8_t charH = SEG_B | SEG_C | SEG_E | SEG_F | SEG_G;
const uint8_t charI = SEG_B | SEG_C;
const uint8_t charL = SEG_D | SEG_E | SEG_F;
const uint8_t charM = SEG_C | SEG_E | SEG_G;
const uint8_t charN = SEG_C | SEG_E | SEG_G;
const uint8_t charO = SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F;
const uint8_t charP = SEG_A | SEG_B | SEG_E | SEG_F | SEG_G;
const uint8_t charR = SEG_E | SEG_G;
const uint8_t charS = SEG_A | SEG_C | SEG_D | SEG_F | SEG_G;
const uint8_t charT = SEG_D | SEG_E | SEG_F | SEG_G;
const uint8_t charU = SEG_B | SEG_C | SEG_D | SEG_E | SEG_F;
const uint8_t charTwo = SEG_A | SEG_B | SEG_D | SEG_E | SEG_G;

// --------------------------------------------------
// Display functions
// --------------------------------------------------

void showText4(
  uint8_t first,
  uint8_t second,
  uint8_t third,
  uint8_t fourth
) {
  const uint8_t segments[] = {
    first, second, third, fourth
  };
  display.setSegments(segments);
}

void showTitleSplash() {
  showText4(charG, charU, charA, charR);
  delay(1200);

  showText4(charT, charE, charS, charT);
  delay(1200);
}

void showConditionLabel(Condition condition) {
  if (condition == conditionBaseline) {
    showText4(charB, charA, charS, charE);
  } else if (condition == conditionHigh) {
    showText4(charH, charI, charG, charH);
  } else {
    showText4(charL, charO, charU, charBlank);
  }
}

void showStart() {
  showText4(charG, charO, charBlank, charBlank);
}

void showEnd() {
  showText4(charE, charN, charD, charBlank);
}

void showError() {
  showText4(charE, charR, charR, charBlank);
}

void showTrialBitResult(uint16_t ones) {
  // Always rewrite all four digits so no previous digit can remain visible.
  // On an exact 100/100 tie, display 0 as the deterministic tie indicator.
  if (ones <= (bitsPerTrial / 2)) {
    showText4(charBlank, charBlank, charBlank, 0x3F);
  } else {
    showText4(charBlank, charBlank, charBlank, 0x06);
  }
}

void showClassification(Classification classification) {
  if (classification == classificationSuccess) {
    showText4(charS, charU, charC, charC);
  } else if (classification == classificationFail) {
    showText4(charF, charA, charI, charL);
  } else if (classification == classificationBaselineFlag) {
    showText4(charF, charL, charA, charG);
  } else {
    showText4(charN, charO, charF, charL);
  }
}

void showTargetLabel(Condition condition) {
  if (condition == conditionLow) {
    showText4(charTwo, charE, charR, charO);
  } else {
    showText4(charO, charN, charE, charS);
  }
}

void showMeanLabel() {
  showText4(charM, charE, charA, charN);
}

void showZeroLabel() {
  showText4(charTwo, charE, charR, charO);
}

void showOnesLabel() {
  showText4(charO, charN, charE, charS);
}

void showCumulativeZeroLabel() {
  showText4(charC, charBlank, charTwo, charO);
}

void showCumulativeOnesLabel() {
  showText4(charC, charBlank, charO, charN);
}

void showZLabel() {
  showText4(charTwo, charS, charC, charO);
}

void showLargeCount(uint32_t value) {
  if (value <= 9999UL) {
    display.showNumberDec(static_cast<int>(value), true, 4, 0);
    delay(resultReadPauseMs);
    return;
  }

  uint32_t highPart = value / 10000UL;
  uint16_t lowPart = static_cast<uint16_t>(value % 10000UL);

  showText4(charH, charI, charL, charO);
  delay(resultReadPauseMs);

  if (highPart > 9999UL) {
    highPart = 9999UL;
  }

  display.showNumberDec(static_cast<int>(highPart), true, 4, 0);
  delay(resultReadPauseMs);

  display.showNumberDec(lowPart, true, 4, 0);
  delay(resultReadPauseMs);
}

// --------------------------------------------------
// Button functions
// --------------------------------------------------

void beginButton() {
  pinMode(buttonPin, INPUT_PULLUP);

  // Do not arm the button until it is released at startup.
  while (digitalRead(buttonPin) == buttonPressedState) {
    delay(5);
  }

  delay(buttonDebounceMs + 10);

  uint8_t initialReading = digitalRead(buttonPin);
  buttonLastReading = initialReading;
  buttonStableReading = initialReading;
  buttonLastChangeMs = millis();
  buttonArmed = (initialReading == buttonReleasedState);
}

bool buttonPressedEvent() {
  uint8_t reading = digitalRead(buttonPin);
  uint32_t now = millis();

  if (reading != buttonLastReading) {
    buttonLastReading = reading;
    buttonLastChangeMs = now;
  }

  if (now - buttonLastChangeMs >= buttonDebounceMs) {
    if (buttonStableReading != buttonLastReading) {
      buttonStableReading = buttonLastReading;

      if (buttonStableReading == buttonReleasedState) {
        buttonArmed = true;
      } else if (
        buttonStableReading == buttonPressedState &&
        buttonArmed
      ) {
        buttonArmed = false;
        return true;
      }
    }
  }

  return false;
}

// --------------------------------------------------
// Potentiometer selection
// --------------------------------------------------

uint16_t readSelectorRaw(uint8_t samples = 16) {
  uint32_t total = 0;

  for (uint8_t i = 0; i < samples; i++) {
    total += analogRead(selectorPin);
    delayMicroseconds(300);
  }

  return static_cast<uint16_t>(total / samples);
}

// Convert the physical ADC reading to a logical left-to-right value.
// ADC 690 at the left becomes logical 0; ADC 0 at the right becomes 690.
uint16_t getLogicalSelectorValue(uint16_t rawValue) {
  if (rawValue > selectorMaxRaw) {
    rawValue = selectorMaxRaw;
  }

  return selectorMaxRaw - rawValue;
}

Condition getInitialCondition(uint16_t rawValue) {
  uint16_t logicalValue = getLogicalSelectorValue(rawValue);

  if (logicalValue < lowBoundary) {
    return conditionLow;
  }

  if (logicalValue < highBoundary) {
    return conditionBaseline;
  }

  return conditionHigh;
}

Condition getHysteresisCondition(uint16_t rawValue) {
  uint16_t logicalValue = getLogicalSelectorValue(rawValue);

  const uint16_t lowEnter = lowBoundary - selectorHysteresis;
  const uint16_t lowExit = lowBoundary + selectorHysteresis;
  const uint16_t highExit = highBoundary - selectorHysteresis;
  const uint16_t highEnter = highBoundary + selectorHysteresis;

  if (currentCondition == conditionLow) {
    if (logicalValue < lowExit) {
      return conditionLow;
    }

    if (logicalValue < highEnter) {
      return conditionBaseline;
    }

    return conditionHigh;
  }

  if (currentCondition == conditionHigh) {
    if (logicalValue > highExit) {
      return conditionHigh;
    }

    if (logicalValue > lowEnter) {
      return conditionBaseline;
    }

    return conditionLow;
  }

  if (logicalValue < lowEnter) {
    return conditionLow;
  }

  if (logicalValue > highEnter) {
    return conditionHigh;
  }

  return conditionBaseline;
}

const char* getConditionName(Condition condition) {
  switch (condition) {
    case conditionBaseline:
      return "BASELINE";
    case conditionHigh:
      return "HIGH";
    case conditionLow:
      return "LOW";
    default:
      return "UNKNOWN";
  }
}

// --------------------------------------------------
// CSV state records
// --------------------------------------------------

void printCsvHeader() {
  Serial.println(
    "recordType,session,accumulationId,condition,selectorRaw,trial,"
    "trialOnes,trialZeros,trialTargetBits,trialExpectedTargetBits,"
    "trialZ,trialTwoSidedP,cumulativeZeros,cumulativeOnes,"
    "cumulativeTargetBits,cumulativeExpectedTargetBits,"
    "cumulativeZ,cumulativeTwoSidedP,interpretation,sessionTargetBits,"
    "sessionExpectedTargetBits,completedTrials,elapsedMs,trngErrors"
  );
}

void printCsvState(const char* recordType, uint16_t rawValue) {
  Serial.print(recordType);
  Serial.print(",");
  Serial.print(sessionNumber);
  Serial.print(",");
  Serial.print(accumulationId);
  Serial.print(",");
  Serial.print(getConditionName(currentCondition));
  Serial.print(",");
  Serial.print(rawValue);

  for (uint8_t field = 5; field < 24; field++) {
    Serial.print(",");
  }

  Serial.println();
}

// --------------------------------------------------
// Condition switching and accumulation reset
// --------------------------------------------------

void changeCondition(Condition newCondition, uint16_t rawValue) {
  if (newCondition == currentCondition) {
    return;
  }

  Condition oldCondition = currentCondition;

  currentCondition = newCondition;
  currentSelectorRaw = rawValue;
  candidateCondition = newCondition;
  candidateStableCount = 0;

  activeStats = ActiveStats();
  accumulationId++;

  Serial.print("Condition changed from ");
  Serial.print(getConditionName(oldCondition));
  Serial.print(" to ");
  Serial.println(getConditionName(currentCondition));

  Serial.println("Active accumulation reset.");

  Serial.print("Potentiometer ADC: ");
  Serial.println(rawValue);

  Serial.print("Logical selector value: ");
  Serial.println(getLogicalSelectorValue(rawValue));

  printCsvState("RESET", rawValue);
  showConditionLabel(currentCondition);
}

void updateSelector() {
  uint32_t now = millis();

  if (now - lastSelectorUpdateMs < selectorPollIntervalMs) {
    return;
  }

  lastSelectorUpdateMs = now;

  uint16_t rawValue = readSelectorRaw(8);

  if (rawValue > selectorMaxRaw) {
    rawValue = selectorMaxRaw;
  }

  currentSelectorRaw = rawValue;

  Condition detectedCondition = getHysteresisCondition(rawValue);

  if (detectedCondition == currentCondition) {
    candidateCondition = currentCondition;
    candidateStableCount = 0;
    return;
  }

  if (detectedCondition != candidateCondition) {
    candidateCondition = detectedCondition;
    candidateStableCount = 1;
  } else if (candidateStableCount < selectorStablePolls) {
    candidateStableCount++;
  }

  if (candidateStableCount >= selectorStablePolls) {
    changeCondition(candidateCondition, rawValue);
  }
}

bool waitWithSelectorMonitoring(
  uint32_t durationMs,
  Condition sessionCondition
) {
  uint32_t startMs = millis();

  while (millis() - startMs < durationMs) {
    updateSelector();

    if (currentCondition != sessionCondition) {
      return false;
    }

    delay(20);
  }

  return true;
}

// --------------------------------------------------
// TRNG and bit counting
// --------------------------------------------------

uint8_t countBits(uint32_t value) {
  uint8_t count = 0;

  while (value != 0) {
    value &= (value - 1UL);
    count++;
  }

  return count;
}

bool readTrialOnes(uint16_t &ones) {
  ones = 0;
  uint32_t randomValue = 0;

  for (uint8_t i = 0; i < 6; i++) {
    if (!TRNG.random32(&randomValue)) {
      return false;
    }

    ones += countBits(randomValue);
  }

  if (!TRNG.random32(&randomValue)) {
    return false;
  }

  ones += countBits(randomValue & 0xFFUL);

  return true;
}

uint16_t getTargetCount(Condition condition, uint16_t ones) {
  if (condition == conditionLow) {
    return bitsPerTrial - ones;
  }

  return ones;
}

// --------------------------------------------------
// Statistical functions
// --------------------------------------------------

double calculateZScore(uint32_t targetBits, uint32_t totalBits) {
  if (totalBits == 0) {
    return 0.0;
  }

  double expected = static_cast<double>(totalBits) / 2.0;
  double standardDeviation = sqrt(static_cast<double>(totalBits)) / 2.0;

  return (static_cast<double>(targetBits) - expected) / standardDeviation;
}

double calculateTwoSidedP(double zScore) {
  return erfc(fabs(zScore) / sqrt(2.0));
}

Classification classifyCumulativeResult(Condition condition, double zScore) {
  if (condition == conditionBaseline) {
    return (fabs(zScore) >= zThreshold)
      ? classificationBaselineFlag
      : classificationNoFlag;
  }

  if (zScore >= zThreshold) {
    return classificationSuccess;
  }

  if (zScore <= -zThreshold) {
    return classificationFail;
  }

  return classificationNoFlag;
}

const char* getClassificationName(Classification classification) {
  switch (classification) {
    case classificationSuccess:
      return "SUCC";
    case classificationFail:
      return "FAIL";
    case classificationBaselineFlag:
      return "FLAG";
    case classificationNoFlag:
    default:
      return "NOFL";
  }
}

// --------------------------------------------------
// CSV trial and session output
// --------------------------------------------------

void printCsvTrial(
  uint8_t trial,
  uint16_t trialOnes,
  uint16_t trialTargetBits,
  double trialZ,
  double trialP,
  uint32_t sessionTargetBits,
  uint32_t completedTrials,
  uint32_t elapsedMs,
  uint32_t trngErrors
) {
  uint32_t cumulativeExpected = activeStats.totalBits / 2UL;

  double cumulativeZ = calculateZScore(
    activeStats.targetBits,
    activeStats.totalBits
  );

  double cumulativeP = calculateTwoSidedP(cumulativeZ);

  Serial.print("TRIAL,");
  Serial.print(sessionNumber);
  Serial.print(",");
  Serial.print(accumulationId);
  Serial.print(",");
  Serial.print(getConditionName(currentCondition));
  Serial.print(",");
  Serial.print(currentSelectorRaw);
  Serial.print(",");
  Serial.print(trial);
  Serial.print(",");
  Serial.print(trialOnes);
  Serial.print(",");
  Serial.print(bitsPerTrial - trialOnes);
  Serial.print(",");
  Serial.print(trialTargetBits);
  Serial.print(",100,");
  Serial.print(trialZ, 6);
  Serial.print(",");
  Serial.print(trialP, 8);
  Serial.print(",");
  Serial.print(activeStats.zeros);
  Serial.print(",");
  Serial.print(activeStats.ones);
  Serial.print(",");
  Serial.print(activeStats.targetBits);
  Serial.print(",");
  Serial.print(cumulativeExpected);
  Serial.print(",");
  Serial.print(cumulativeZ, 6);
  Serial.print(",");
  Serial.print(cumulativeP, 8);
  Serial.print(",RUNNING,");
  Serial.print(sessionTargetBits);
  Serial.print(",");
  Serial.print(completedTrials * 100UL);
  Serial.print(",");
  Serial.print(completedTrials);
  Serial.print(",");
  Serial.print(elapsedMs);
  Serial.print(",");
  Serial.println(trngErrors);
}

void printCsvSessionSummary(
  uint32_t sessionTargetBits,
  uint32_t completedTrials,
  uint32_t elapsedMs,
  uint32_t trngErrors,
  double cumulativeZ,
  double cumulativeP,
  Classification classification
) {
  uint32_t cumulativeExpected = activeStats.totalBits / 2UL;

  Serial.print("SESSION,");
  Serial.print(sessionNumber);
  Serial.print(",");
  Serial.print(accumulationId);
  Serial.print(",");
  Serial.print(getConditionName(currentCondition));
  Serial.print(",");
  Serial.print(currentSelectorRaw);
  Serial.print(",,,,,,,");
  Serial.print(activeStats.zeros);
  Serial.print(",");
  Serial.print(activeStats.ones);
  Serial.print(",");
  Serial.print(activeStats.targetBits);
  Serial.print(",");
  Serial.print(cumulativeExpected);
  Serial.print(",");
  Serial.print(cumulativeZ, 6);
  Serial.print(",");
  Serial.print(cumulativeP, 8);
  Serial.print(",");
  Serial.print(getClassificationName(classification));
  Serial.print(",");
  Serial.print(sessionTargetBits);
  Serial.print(",");
  Serial.print(completedTrials * 100UL);
  Serial.print(",");
  Serial.print(completedTrials);
  Serial.print(",");
  Serial.print(elapsedMs);
  Serial.print(",");
  Serial.println(trngErrors);
}

// --------------------------------------------------
// Session result screens
// --------------------------------------------------

void showSessionResults(
  Condition sessionCondition,
  uint32_t sessionZeros,
  uint32_t sessionOnes
) {
  if (activeStats.totalBits == 0) {
    showError();
    delay(resultReadPauseMs);
    return;
  }

  double cumulativeZ = calculateZScore(
    activeStats.targetBits,
    activeStats.totalBits
  );

  double cumulativeP = calculateTwoSidedP(cumulativeZ);
  Classification classification =
    classifyCumulativeResult(sessionCondition, cumulativeZ);

  double scaledZ = round(fabs(cumulativeZ) * 100.0);
  uint16_t zTimes100 = (scaledZ > 9999.0)
    ? 9999
    : static_cast<uint16_t>(scaledZ);

  Serial.println();
  Serial.println("GUAR tester CUMULATIVE RESULTS");
  Serial.print("Condition: ");
  Serial.println(getConditionName(sessionCondition));
  Serial.print("Session zeros: ");
  Serial.println(sessionZeros);
  Serial.print("Session ones: ");
  Serial.println(sessionOnes);
  Serial.print("Accumulated zeros: ");
  Serial.println(activeStats.zeros);
  Serial.print("Accumulated ones: ");
  Serial.println(activeStats.ones);
  Serial.print("Total accumulated bits: ");
  Serial.println(activeStats.totalBits);
  Serial.print("Expected zeros: ");
  Serial.println(activeStats.totalBits / 2UL);
  Serial.print("Expected ones: ");
  Serial.println(activeStats.totalBits / 2UL);
  Serial.print("Signed Z-score (target bits): ");
  Serial.println(cumulativeZ, 6);
  Serial.print("Absolute Z-score: ");
  Serial.println(fabs(cumulativeZ), 6);
  Serial.print("Two-sided p-value: ");
  Serial.println(cumulativeP, 8);
  Serial.print("Interpretation: ");
  Serial.println(getClassificationName(classification));
  Serial.println("Threshold: |Z| >= 1.645");
  Serial.println("Statistical deviation does not establish an intention effect.");

  // Session counts: zeros and ones from this 60-trial session.
  showZeroLabel();
  delay(resultReadPauseMs);
  showLargeCount(sessionZeros);

  showOnesLabel();
  delay(resultReadPauseMs);
  showLargeCount(sessionOnes);

  // Accumulated counts since this condition was selected.
  showCumulativeZeroLabel();
  delay(resultReadPauseMs);
  showLargeCount(activeStats.zeros);

  showCumulativeOnesLabel();
  delay(resultReadPauseMs);
  showLargeCount(activeStats.ones);

  showConditionLabel(sessionCondition);
  delay(resultReadPauseMs);

  showTargetLabel(sessionCondition);
  delay(resultReadPauseMs);
  showLargeCount(activeStats.targetBits);

  showMeanLabel();
  delay(resultReadPauseMs);
  showLargeCount(activeStats.totalBits / 2UL);

  showZLabel();
  delay(resultReadPauseMs);
  display.showNumberDec(zTimes100, true, 4, 0);
  delay(resultReadPauseMs);

  showClassification(classification);
  delay(resultReadPauseMs);
}

// --------------------------------------------------
// Session execution
// --------------------------------------------------

void runSession() {
  sessionNumber++;

  const Condition sessionCondition = currentCondition;

  uint32_t sessionTargetBits = 0;
  uint32_t sessionZeros = 0;
  uint32_t sessionOnes = 0;
  uint32_t completedTrials = 0;
  uint32_t trngErrors = 0;
  uint32_t sessionStartMs = millis();

  Serial.println();
  Serial.println("========================================");
  Serial.println("GUAR tester SESSION START");

  Serial.print("Condition: ");
  Serial.println(getConditionName(sessionCondition));

  Serial.print("Potentiometer ADC: ");
  Serial.println(currentSelectorRaw);

  if (sessionCondition == conditionHigh) {
    Serial.println("Intention: produce more 1 bits.");
  } else if (sessionCondition == conditionLow) {
    Serial.println("Intention: produce more 0 bits.");
  } else {
    Serial.println("Baseline: no intention.");
  }

  showConditionLabel(sessionCondition);

  if (!waitWithSelectorMonitoring(1000, sessionCondition)) {
    Serial.println("Session cancelled: selector condition changed.");
    return;
  }

  showStart();

  if (!waitWithSelectorMonitoring(700, sessionCondition)) {
    Serial.println("Session cancelled: selector condition changed.");
    return;
  }

  for (uint8_t trial = 1; trial <= trialsPerSession; trial++) {
    if (currentCondition != sessionCondition) {
      Serial.println("Session stopped: selector condition changed.");
      return;
    }

    uint16_t trialOnes = 0;

    if (!readTrialOnes(trialOnes)) {
      trngErrors++;

      Serial.print("TRNG ERROR at trial ");
      Serial.println(trial);

      break;
    }

    uint16_t trialTargetBits =
      getTargetCount(sessionCondition, trialOnes);

    uint16_t trialZeros = bitsPerTrial - trialOnes;
    sessionTargetBits += trialTargetBits;
    sessionZeros += trialZeros;
    sessionOnes += trialOnes;
    completedTrials++;

    activeStats.targetBits += trialTargetBits;
    activeStats.totalBits += bitsPerTrial;
    activeStats.zeros += trialZeros;
    activeStats.ones += trialOnes;
    activeStats.trials++;

    double trialZ = calculateZScore(trialTargetBits, bitsPerTrial);
    double trialP = calculateTwoSidedP(trialZ);
    uint32_t elapsedMs = millis() - sessionStartMs;

    printCsvTrial(
      trial,
      trialOnes,
      trialTargetBits,
      trialZ,
      trialP,
      sessionTargetBits,
      completedTrials,
      elapsedMs,
      trngErrors
    );

    showTrialBitResult(trialOnes);

    if (!waitWithSelectorMonitoring(trialIndicatorMs, sessionCondition)) {
      Serial.println(
        "Session stopped: selector changed. Accumulation reset."
      );
      return;
    }
  }

  uint32_t elapsedMs = millis() - sessionStartMs;

  double cumulativeZ = calculateZScore(
    activeStats.targetBits,
    activeStats.totalBits
  );

  double cumulativeP = calculateTwoSidedP(cumulativeZ);

  Classification classification =
    classifyCumulativeResult(sessionCondition, cumulativeZ);

  printCsvSessionSummary(
    sessionTargetBits,
    completedTrials,
    elapsedMs,
    trngErrors,
    cumulativeZ,
    cumulativeP,
    classification
  );

  showEnd();
  delay(1200);

  showSessionResults(sessionCondition, sessionZeros, sessionOnes);

  Serial.println();
  Serial.println("GUAR tester READY");
  Serial.println("Turn potentiometer to select a condition.");
  Serial.println("Press button to start another session.");
}

// --------------------------------------------------
// Arduino setup and loop
// --------------------------------------------------

void setup() {
  Serial.begin(115200);

  display.setBrightness(displayBrightness);
  display.clear();

  pinMode(selectorPin, INPUT);
  analogReadResolution(10);

  Serial.println("// GUAR tester - Based on the PEAR Laboratory experiments");
  Serial.println("// Roni Bandini, 10/2026, GP License");
  Serial.println("// UNO R4 minima, 7-segment display, push button and potentiometer");
  Serial.println("// Pot range: ADC 0-690; left=LOW, center=BASELINE, right=HIGH");
  Serial.println("// Button: D4 to GND, using INPUT_PULLUP");
  Serial.println("// HIGH counts 1s; LOW counts 0s; BASELINE counts 1s without intention.");

  if (!TRNG.begin()) {
    Serial.println("// ERROR: TRNG initialization failed.");
    showError();

    while (true) {
      delay(1000);
    }
  }

  showTitleSplash();
  beginButton();

  currentSelectorRaw = readSelectorRaw(16);

  if (currentSelectorRaw > selectorMaxRaw) {
    currentSelectorRaw = selectorMaxRaw;
  }

  currentCondition = getInitialCondition(currentSelectorRaw);
  candidateCondition = currentCondition;
  candidateStableCount = 0;

  printCsvHeader();

  Serial.println();
  Serial.println("GUAR tester READY");

  Serial.print("Initial potentiometer ADC: ");
  Serial.println(currentSelectorRaw);

  Serial.print("Logical selector value: ");
  Serial.println(getLogicalSelectorValue(currentSelectorRaw));

  Serial.print("Selected condition: ");
  Serial.println(getConditionName(currentCondition));

  Serial.println("Move the potentiometer to change condition.");
  Serial.println("Press the button to start GUAR tester.");

  printCsvState("READY", currentSelectorRaw);
  showConditionLabel(currentCondition);
}

void loop() {
  updateSelector();

  if (buttonPressedEvent()) {
    runSession();
  }
}
