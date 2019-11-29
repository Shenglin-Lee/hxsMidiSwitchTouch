
Bounce fsDebouncers[footSwitchCount];

MidiMessage *individualSwitchAction[footSwitchCount][totalPresetPages][individualActionTypeCount];//array to hold pointers for all action types / pages / footswitches
MidiMessage *comboSwitchAction[footSwitchCount][footSwitchCount][totalPresetPages];//array to hold pointers for 2x switch combination press

//default config for footswitch single tap actions
MidiMessage *page0SingleActions[] = {&hxsSnap1, &hxsSnap2, &hxsSnap3, &hxsPresetUpMacro, 
                            &hxsFS1, &hxsFS2, &hxsFS3, &hxsPresetDownMacro};
MidiMessage *page1SingleActions[] = {&hxsSnap1, &hxsSnap2, &hxsSnap3, &hxsFS5, 
                        &hxsFS1, &hxsFS2, &hxsFS3, &hxsFS4};
MidiMessage *page2SingleActions[] = {&hxsSnap1, &hxsSnap2, &hxsSnap3, &hxsPresetUpMacro, 
                        &hxsAllBypass, &hxsNoBypass, &hxsTuner, &hxsPresetDownMacro};
MidiMessage *page3SingleActions[] = {&hxsLooperOverdub, &hxsLooperRec, &hxsLooperForward, &hxsLooperReverse, 
                        &hxsLooperStop, &hxsLooperPlay, &hxsLooperPlayOnce, &hxsLooperUndoRedo};
MidiMessage *page4SingleActions[] = {&hxsLooperOverdub, &hxsLooperRec, &hxsLooperFullSpeed, &hxsLooperHalfSpeed,
                        &hxsLooperStop, &hxsLooperPlay, &hxsLooperPlayOnce, &hxsLooperUndoRedo};
    
    


void setupFootSwitches() { //configure pins and attach debouncers
  for (int i=0; i<footSwitchCount; i++) {
    fsDebouncers[i] = Bounce();
    pinMode(footSwitchPin[i], INPUT_PULLUP);
    fsDebouncers[i].attach(footSwitchPin[i]);
    fsDebouncers[i].interval(5); //interval in ms
    footSwitchCurrentValue[i] = footSwitchUp;
    
    //footSwitchLastValue[i] = footSwitchUp; // don't think we are using this any more.... probably will when long and double click implemented though
    
  }//for loop
}//setupFootSwitches()

void setupFootSwitchActions() { //initialise individualSwitchAction array with preset config
    //start by initialising all action pointers to &blankMidiMsg
    for (int fsA=0; fsA<footSwitchCount; fsA++) { //iterate over all footswitches
        for (int pg=0; pg<totalPresetPages; pg++) { //iterate over all preset pages
            for (int act=0; act<individualActionTypeCount; act++) { //iterate over all switch action types
                individualSwitchAction[fsA][pg][act]=&blankMidiMsg;
            }//for(act)
        }//for (pg)
        for (int fsB =0; fsB<footSwitchCount; fsB++) { //iterate over all switches again so all combinations are covered
            setComboAction(-1,fsA,fsB,&blankMidiMsg); //set blank action on all pages
        }//for(fsB)
    }//for (fsA)
    
    //set predefined actions
    setPageAction(0,SINGLE,page0SingleActions);
    setPageAction(1,SINGLE,page1SingleActions);
    setPageAction(2,SINGLE,page2SingleActions);
    setPageAction(3,SINGLE,page3SingleActions);
    setPageAction(4,SINGLE,page4SingleActions);
    
    //set predefined long press actions
    individualSwitchAction[0][0][LONG]=&hxsTuner;
    
    //set predefined combination actions
    
    setComboAction(-1, 0, 4, &hxsPresetDownMacro);
    setComboAction(-1, 3, 7, &hxsPresetUpMacro);
    setComboAction(-1, 2, 3, &hxsTuner);
    
    
}//setupFootSwitchActions()

void setPageAction(int page, individualActionType action, MidiMessage *msg[footSwitchCount]) { //set all FS actions for a single page and action type
    for (int fs=0; fs<footSwitchCount; fs++) {
        individualSwitchAction[fs][page][action]=msg[fs];
    }
}//setPageAction

void setComboAction(int page, int fsA, int fsB, MidiMessage *msg) { //set an action for a 2x switch combination press
    //we need to sort the switches by index so we never have conficting settings with switch indexes swapped
    int lowestSwitchIndex; 
    int highestSwitchIndex;
    if (fsA<fsB) {
        lowestSwitchIndex=fsA;
        highestSwitchIndex=fsB;
        }
    else {
        lowestSwitchIndex=fsB;
        highestSwitchIndex=fsA;
        }
    if (page==-1) {//then apply this combo to all pages
        for (int p=0; p<totalPresetPages; p++) {
            comboSwitchAction[lowestSwitchIndex][highestSwitchIndex][p]=msg;
        }//for loop
    }//if (page==-1)
    else {//then only apply it to a single page
        comboSwitchAction[lowestSwitchIndex][highestSwitchIndex][page]=msg;
    }//else
}//setComboAction

void updateScreenLabels() { //update Nextion screen with text labels from current page single click action MidiMessages
    for (int sw =0; sw<footSwitchCount;sw++) {
        for (int line=0; line<labelLinesPerSwitch;line++) {
            footSwitchScreenLabels[sw][line]->setText( individualSwitchAction[sw][currentPage][SINGLE]->label[line] );
        }//for(line)
    } //for (sw)
}//updateScreenLabels()

bool fsIndexIsValid (int fsIndex){ //function to check if a given int is a valid footswitch index
        if ((fsIndex >= 0) && (fsIndex < footSwitchCount) ) { //then switchIndex is valid
            return true;
            }
        else {
            return false;
            }   
}// fsIndexIsValid



void readFootSwitches() { //check footswitches for any input
    const bool WAIT_FOR_LONG_PRESS = true; // if true, prevents a long press on a switch triggering single click and long press events. If false, single click event will be triggered immediately, and long press will follow it if switch is held down for long enough. Recommend setting this to false if timing is sensitive.
    const int longPressMillis = 1000; //how long a single switch must be held down for to trigger a long press event
    const int doubleClickMillis = 500; //maximum time between consecutive clicks on a switch to trigger a double click event
    static int longPressWatcher = -1; //set to switch index of any potential long press candidate, or -1 if no valid candidate.
    static int totalFootSwitchesDown =0; //for tracking how many switches are pressed so we know whether to check for switch combination actions
    static unsigned long lastPressMillis[footSwitchCount] ={0}; //the last time the switch was pressed
    static unsigned long lastLastPressMillis[footSwitchCount] ={0}; //the last but one time the switch was pressed
    static unsigned long lastReleaseMillis[footSwitchCount] ={0}; //the last time the switch was released
    static unsigned long lastLongPressSentMillis[footSwitchCount]={0}; //the time a long press message was last sent
    unsigned long currentMillis = millis(); //time at start of loop
    static int comboActive = false; //set true if >1 switches pressed. ignore all further 
    static int comboUnsent = false; 
    
    for (int i=0; i<footSwitchCount; i++) { // iterate over all the switches
        fsDebouncers[i].update(); //update the Bounce debouncer object for each switch
        if (fsDebouncers[i].fell()) { //then footswitch i has been pressed
            footSwitchCurrentValue[i] = footSwitchDown;
            totalFootSwitchesDown++;//increase counter for how many switches we know are currently down
            lastLastPressMillis[i] = lastPressMillis[i];
            lastPressMillis[i] = currentMillis;
            if (comboActive==false) { //then we may need to act on a switch press
                if (totalFootSwitchesDown>1) {
                    comboActive = true;
                    if (totalFootSwitchesDown==2) {// check for valid combo and act accordingly
                        int fsA =-1;
                        int fsB =-1;
                        for (int fs =0; fs<footSwitchCount; fs++) {
                            if (footSwitchCurrentValue[fs]==footSwitchDown) { // we have found a pressed switch
                                if (fsA==-1) { //then this is the first switch found
                                    fsA=fs;
                                }
                                else { //this is the second switch found
                                    fsB=fs;
                                }//else
                            }//if (footSwitchCurrentValue[fs]==footSwitchDown)
                        }//for (int fs =0; fs<footSwitchCount; fs++)
                        comboSwitchAction[fsA][fsB][currentPage]->sendToMidi();//send MidiMessage for pressed combo
                    }//if (totalFootSwitchesDown==2)
                }//if (totalFootSwitchesDown>1))
                else { //only one switch is pressed - test for single/double click actions
                    if ((lastPressMillis-lastLastPressMillis) < doubleClickMillis) { // then trigger double click action
                        individualSwitchAction[i][currentPage][DOUBLE]->sendToMidi(); //send MidiMessage for double press action 
                    } //if ((lastPressMillis-lastLastPressMillis) < doubleClickMillis)
                    else { //could be single click or start of long press
                        if (WAIT_FOR_LONG_PRESS) {
                            longPressWatcher=i; //set this to keep track of a potential long press candidate
                        }//if (WAIT_FOR_LONG_PRESS)
                        else { //send single click action immediately
                            individualSwitchAction[i][currentPage][SINGLE]->sendToMidi(); //send MidiMessage for single press action 
                        }//else
                    }
                }//else
            }//if (comboActive==false)
            
        }//if (fsDebouncers[i].fell())
        if (fsDebouncers[i].rose()) { //then footswitch i has been released
            footSwitchCurrentValue[i] = footSwitchUp;
            totalFootSwitchesDown--;//increase counter for how many switches we know are currently down
            lastReleaseMillis[i] = currentMillis;
            
            
            if (comboActive==true) { //then we just need to check if all switches have been released
                if (totalFootSwitchesDown==0) {
                    comboActive=false;
                } //if (totalFootSwitchesDown==0)
            }//if (comboActive==true)
            else { // we need to check if we were waiting for a short press
                if (WAIT_FOR_LONG_PRESS) { //then we didn't send short press action immediately
                    if ((lastReleaseMillis[i]-lastPressMillis[i])<longPressMillis) { //switch was depressed for less than longPressMillis
                        individualSwitchAction[i][currentPage][SINGLE]->sendToMidi(); //send MidiMessage for single press action
                    }//if ((lastReleaseMillis[i]-lastPressMillis[i])<longPressMillis)
                }//if (WAIT_FOR_LONG_PRESS)
            }//else
        } //if (fsDebouncers[i].rose())
        
        if ((lastPressMillis[i] > lastReleaseMillis[i]) &&(currentMillis-lastPressMillis[i] > longPressMillis)) { //then switch has been held down long enough to trigger a long press
            if (not comboActive) { // (because we're ignoring switch presses while comboActive is still true)
                if (lastPressMillis[i] > lastLongPressSentMillis[i]) { //then we haven't yet sent a long press event
                    individualSwitchAction[i][currentPage][LONG]->sendToMidi(); //send MidiMessage for long press action
                    lastLongPressSentMillis[i] = currentMillis; // update last long press sent tracker so we don't send duplicate messages if switch is still held down next loop
                } //if (lastPressMillis > lastLongPressSentMillis)
            }//if (not comboActive)
        }//if ((lastPressMillis-lastReleaseMillis)>longPressMillis)
    }//for (int i=0; i<footSwitchCount; i++)
}//readFootswitches()

