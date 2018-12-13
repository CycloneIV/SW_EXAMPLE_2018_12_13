// Copyright: Microsoft 2009
//
// Author: Ken Eguro and Sandro Forin
// Includes code written by 2009 MSR intern Rene Mueller.
//
// Created: 10/23/09 
//
// Version: 1.1
// 
// Changelog:
// Updated to fix bugs and compile from .lib
//
// Description:
// Example test program.
// Test #1 - Read and write parameter registers
// Test #2 - Soft reset of user logic
// Test #3 - Test circuit:	send random input bytes to the FPGA 
//							multiply by 3
//							retrieve results
// Test #4 - Test circuit: same as test #3, only use new random numbers & write/execute command
// Test #5 - Test circuit: same as test #4, only show how to handle small output error of write/execute command
// Test #6 - Write bandwidth test
// Test #7 - Read bandwidth test
//----------------------------------------------------------------------------
//
#include <windows.h>
#include <WinIoctl.h>
#include <setupapi.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <iomanip>
#include <assert.h>
#include <list>
#include <vector>
#include <time.h>
#include <direct.h>

#include <stdlib.h>
#include "sirc.h"
#include "sirc_error.h"
#include "sirc_util.h"
#include "log.h"
#include "display.h"

using namespace std;
using namespace System;
using namespace System::IO;
using namespace System::Collections;

#define INPUT_BUFFER_INPUT_ADDRESS_WIDTH 17

#define OUTPUT_BUFFER_OUTPUT_ADDRESS_WIDTH 14   ////13

#define ITERNO 100

#define CHALLENGE_NUM_PER_ITER 128

#define REPEAT_NUM_PER_CHALLENGE 64

#define TUNING_LEVEL 0

#define CHALLENGE_WIDTH 16

#define RESPONSE_WIDTH 2   ////1

#define DEVICE 220370

void error(string inErr){
	cerr << "Error:" << endl;
	cerr << "\t" << inErr << endl;

    PrintZeLog();
	system("PAUSE");
}

DWORD dwDisplayThreadID = 0;
typedef char strbuf[64];
strbuf lines[4];
bool displayUpdated = false;

uint8_t genOneSequence (int num) {
	switch (num ) {
	case 0: return 0;
	case 1: return 128;
	case 2: return 192;
	case 3: return 224;
	case 4: return 240;
	case 5: return 248;
	case 6: return 252;
	case 7: return 254;
	case 8: return 255;
	}
}

void delay (int time){
	clock_t now = clock();
	while(clock() - now < time);
}



void generate_challenge(int parameter, bool pol, uint8_t *tempBuffer){
	int i;

	parameter = abs(parameter);

	if(parameter<1){
		for(i=0; i<CHALLENGE_WIDTH; i++){
			tempBuffer[i] = rand()%256;
			//tempBuffer[i+CHALLENGE_WIDTH] = tempBuffer[i];
		}
	}

	else if(parameter<9){
		tempBuffer[CHALLENGE_WIDTH*((pol)?1:0)] = genOneSequence(parameter);
		for(i=1; i<CHALLENGE_WIDTH; i++){
			tempBuffer[i] = rand()%256;
			tempBuffer[i+CHALLENGE_WIDTH] = tempBuffer[i];
		}
	}

	else if (parameter<17){
		tempBuffer[CHALLENGE_WIDTH*((pol)?1:0)] = 255;
		tempBuffer[CHALLENGE_WIDTH*((pol)?1:0)+1] = genOneSequence(parameter-8);
		for(i=2; i<CHALLENGE_WIDTH; i++){
			tempBuffer[i] = rand()%256;
			tempBuffer[i+CHALLENGE_WIDTH] = tempBuffer[i];
		}
	}

	else if (parameter<25){
		tempBuffer[CHALLENGE_WIDTH*((pol)?1:0)] = 255;
		tempBuffer[CHALLENGE_WIDTH*((pol)?1:0)+1] = 255;
		tempBuffer[CHALLENGE_WIDTH*((pol)?1:0)+2] = genOneSequence(parameter-16);
		for(i=3; i<CHALLENGE_WIDTH; i++){
			tempBuffer[i] = rand()%256;
			tempBuffer[i+CHALLENGE_WIDTH] = tempBuffer[i];
		}
	}

	else if (parameter <33){
		tempBuffer[CHALLENGE_WIDTH*((pol)?1:0)] = 255;
		tempBuffer[CHALLENGE_WIDTH*((pol)?1:0)+1] = 255;
		tempBuffer[CHALLENGE_WIDTH*((pol)?1:0)+2] = 255;
		tempBuffer[CHALLENGE_WIDTH*((pol)?1:0)+3] = genOneSequence(parameter-24);
		for(i=4; i<CHALLENGE_WIDTH; i++){
			tempBuffer[i] = rand()%256;
			tempBuffer[i+CHALLENGE_WIDTH] = tempBuffer[i];
		}
	}

	else if (parameter<41){
		tempBuffer[CHALLENGE_WIDTH*((pol)?1:0)] = 255;
		tempBuffer[CHALLENGE_WIDTH*((pol)?1:0)+1] = 255;
		tempBuffer[CHALLENGE_WIDTH*((pol)?1:0)+2] = 255;
		tempBuffer[CHALLENGE_WIDTH*((pol)?1:0)+3] = 255;
		tempBuffer[CHALLENGE_WIDTH*((pol)?1:0)+4] = genOneSequence(parameter-32);
		for(i=5; i<CHALLENGE_WIDTH; i++){
			tempBuffer[i] = rand()%256;
			tempBuffer[i+CHALLENGE_WIDTH] = tempBuffer[i];
		}
	}
	
	else if (parameter<49){
		tempBuffer[CHALLENGE_WIDTH*((pol)?1:0)] = 255;
		tempBuffer[CHALLENGE_WIDTH*((pol)?1:0)+1] = 255;
		tempBuffer[CHALLENGE_WIDTH*((pol)?1:0)+2] = 255;
		tempBuffer[CHALLENGE_WIDTH*((pol)?1:0)+3] = 255;
		tempBuffer[CHALLENGE_WIDTH*((pol)?1:0)+4] = 255;
		tempBuffer[CHALLENGE_WIDTH*((pol)?1:0)+5] = genOneSequence(parameter-40);
		for(i=6; i<CHALLENGE_WIDTH; i++){
			tempBuffer[i] = rand()%256;
			tempBuffer[i+CHALLENGE_WIDTH] = tempBuffer[i];
		}
	}
}

int crp_copytofile(uint8_t *input1, uint8_t *input2, int num1, int num2,string filename, int circu )
{
	FILE *fp1, *fp2;
	stringstream ss;
	string circulation_crp;
	ss << circu;
	ss >> circulation_crp;
	string filename_input = "challenge_" + filename + ".bin";
	string filename_output = "response_" + filename + "_" + circulation_crp +".bin";
	

	fp1 = fopen(filename_input.c_str(), "wb");
	if( input1!= NULL)
	{
		fwrite (input1 , 1 , num1, fp1 );
	}
	fclose(fp1);
	

	fp2 = fopen(filename_output.c_str(), "ab");
	if( input2!= NULL)
	{
		fwrite (input2 , 1 , num2, fp2 );
	}
	fclose(fp2);
	return 0;
}

string convertInt(int number)
{
	stringstream ss;//create a stringstream
	ss << number;//add number to the stream
	return ss.str();//return a string with the contents of the stream
}

DWORD WINAPI displayThreadProc(LPVOID lpParam)
{
	SW_Example::display displayObj;
	displayObj.Text = "SIRC Test Application";
	displayObj.Show();
	
	MSG tempMsg;
	while(1){
		if(::GetMessage(&tempMsg, NULL, 0, 0) > 0){
			if(tempMsg.message == WM_PAINT){
                String ^t0, ^t1, ^t2, ^t3;
                t0 = gcnew String(lines[0]);
                t1 = gcnew String(lines[1]);
                t2 = gcnew String(lines[2]);
                t3 = gcnew String(lines[3]);
				displayObj.updateStrings(t0, t1, t2, t3);
				displayUpdated = true;
			}
            ::TranslateMessage(&tempMsg); 
            ::DispatchMessage(&tempMsg);
            if ((tempMsg.message == WM_DESTROY) ||
                (tempMsg.message == WM_CLOSE) ||
                (tempMsg.message == WM_QUIT))
                break;
		}
		else{
			break;
		}
	}
    return 0;
}



int main(int argc, char* argv[])
{
	//Test harness variables
	SIRC *SIRC_P;
	uint8_t FPGA_ID[6];

	uint32_t driverVersion = 0;

	uint32_t numOps = 0;

	string filename_crp;

	string circualtion_crp;

	uint32_t INPUT_OUTPUT_BUFFER_RATIO;

	//Speed testing variables
	DWORD start, end;

	std::ostringstream tempStream;

	DWORD dwDisplayThreadID;

	clock_t launch ,finish;

	double duration;

	bool   flag = 1;
	
	#ifndef BUGHUNT 
	    StartLog();
	#endif

	launch = clock();
	for(int circulation = 0;circulation <10000;circulation++)
	{
		
		
		cout << "****USING DEFAULT MAC ADDRESS - AA:AA:AA:AA:AA:AA" << endl;
		FPGA_ID[0] = 0xAA;
		FPGA_ID[1] = 0xAA;
		FPGA_ID[2] = 0xAA;
		FPGA_ID[3] = 0xAA;
		FPGA_ID[4] = 0xAA;
		FPGA_ID[5] = 0xAA;

		//**** Set up communication with FPGA
		//Create communication object
		SIRC_P = openSirc(FPGA_ID,driverVersion);
		//Make sure that the constructor didn't run into trouble
	    if (SIRC_P == NULL){
			tempStream << "Unable to find a suitable SIRC driver or unable to ";
			error(tempStream.str());
	    }
		if(SIRC_P->getLastError() != 0){
			tempStream << "Constructor failed with code " << (int) SIRC_P->getLastError();
			error(tempStream.str());
		}

	    //Get runtime parameters, for what we wont change.
	    SIRC::PARAMETERS params;
	    if (!SIRC_P->getParameters(&params,sizeof(params))){
			tempStream << "Cannot getParameters from SIRC interface, code " << (int) SIRC_P->getLastError();
			error(tempStream.str());
	    }

	    //These MUST match the buffer sizes in the hw design.
	    params.maxInputDataBytes  = 1<<INPUT_BUFFER_INPUT_ADDRESS_WIDTH;
	    params.maxOutputDataBytes = 1<<OUTPUT_BUFFER_OUTPUT_ADDRESS_WIDTH;

		INPUT_OUTPUT_BUFFER_RATIO = params.maxInputDataBytes / params.maxOutputDataBytes;

	    if (!SIRC_P->setParameters(&params,sizeof(params))){
			tempStream << "Cannot setParameters on SIRC interface, code " << (int) SIRC_P->getLastError();
			error(tempStream.str());
	    }

	    //Fill up the input buffer, unless.
	    if (numOps == 0){
	       numOps = min(params.maxInputDataBytes, params.maxOutputDataBytes);
		}

		if(!SIRC_P->sendReset()){
			tempStream << "Reset failed with code " << (int) SIRC_P->getLastError();
			error(tempStream.str());
		}

		//This example circuit takes a few parameters:
		//	Param Register 0 - number of input bytes
		//	Param Register 1 - 32-bit multipler
		//	Input buffer - N bytes to be multipled
		//	Expected output - N bytes equal to (input values * multipler) % 256
		
		//To reduce calls to file writing functions, we declare a local storage
		// uint8_t *input_storage;
		// uint8_t *output_storage;

		// input_storage = (uint8_t *) malloc(sizeof(uint8_t) * numOps * ITERNO * INPUT_OUTPUT_BUFFER_RATIO);
		// assert(input_storage);
		// output_storage =  (uint8_t *) malloc(sizeof(uint8_t) * numOps * ITERNO);
		// assert(output_storage);
		

		for (int iterOp=0; iterOp < ITERNO; iterOp++) 
		{
			//Input buffer
			uint8_t *inputValues;
			//Output buffer
			uint8_t *outputValues;
			uint8_t *tempBuffer;

			inputValues = (uint8_t *) malloc(sizeof(uint8_t) * numOps * INPUT_OUTPUT_BUFFER_RATIO);
			assert(inputValues);
		
			tempBuffer = (uint8_t *) malloc(sizeof(uint8_t) * CHALLENGE_WIDTH);
			assert(tempBuffer);

			cout << "CIRCULATION N0."<<circulation<<"***READ/WRITE ITERATION NO. " << iterOp << endl; 
			memset(inputValues, 0, numOps * INPUT_OUTPUT_BUFFER_RATIO);
		
			
			
			bool polarity;
			if (TUNING_LEVEL > 0) {
				polarity = 1;
			}
			else {
				polarity = 0;
			}
			
			
			

			for (int i=0; i<CHALLENGE_NUM_PER_ITER; i++){
				memset(tempBuffer, 0, CHALLENGE_WIDTH);
				generate_challenge(TUNING_LEVEL, polarity, tempBuffer);
				for (int j=0; j<REPEAT_NUM_PER_CHALLENGE; j++){
					memcpy(inputValues + (i*REPEAT_NUM_PER_CHALLENGE+j)*CHALLENGE_WIDTH, tempBuffer, CHALLENGE_WIDTH);
				}
			}
			
			//Set parameter register 0 to the number of operations
			if(!SIRC_P->sendParamRegisterWrite(0, numOps / RESPONSE_WIDTH)){
				tempStream << "Parameter register write failed with code " << (int) SIRC_P->getLastError();
				error(tempStream.str());
			}

			//Set parameter register 1 to a multiplier of 3
			if(!SIRC_P->sendParamRegisterWrite(1, 2)){
				tempStream << "Parameter register write failed with code " << (int) SIRC_P->getLastError();
				error(tempStream.str());
			}

			

			//Next, send the input data
			//Start writing at address 0
			if(!SIRC_P->sendWrite(0, numOps * INPUT_OUTPUT_BUFFER_RATIO, inputValues)){
				tempStream << "Write to FPGA failed with code " << (int) SIRC_P->getLastError();
				error(tempStream.str());
			}

			//Set the run signal
			if(!SIRC_P->sendRun()){
				tempStream << "Run command failed with code " << (int) SIRC_P->getLastError();
				error(tempStream.str());
			}

			if(!SIRC_P->waitDone(4000)){
				tempStream << "Wait till done failed with code " << (int) SIRC_P->getLastError();
				error(tempStream.str());
			}
			
			if(flag){
				outputValues = (uint8_t *) malloc(sizeof(uint8_t) * numOps);
				assert(outputValues);
				memset(outputValues, 0, numOps);
				//Read the data back
				if(!SIRC_P->sendRead(0, numOps, outputValues)){
					tempStream << "Read from FPGA failed with code " << (int) SIRC_P->getLastError();
					error(tempStream.str());
				}			
			// for(uint32_t k = 0; k < numOps * INPUT_OUTPUT_BUFFER_RATIO; k++) {
			// 	input_storage[iterOp * numOps * INPUT_OUTPUT_BUFFER_RATIO + k] = inputValues[k];
			// }
			// memmove (output_storage + (iterOp * numOps), outputValues, numOps);
				filename_crp = convertInt(8 * CHALLENGE_WIDTH) + "_" + convertInt(ITERNO * CHALLENGE_NUM_PER_ITER * REPEAT_NUM_PER_CHALLENGE) + "_" + convertInt(TUNING_LEVEL) + "_" + convertInt(DEVICE);
				crp_copytofile(inputValues, outputValues, numOps*INPUT_OUTPUT_BUFFER_RATIO, numOps, filename_crp, circulation);
				free(outputValues);
			}
			else{

			}

			free(inputValues);
			
			free(tempBuffer);

			delay(5);
		}

		//Copy the results in files....
		
		
		// filename_crp = convertInt(8 * CHALLENGE_WIDTH) + "_" + convertInt(ITERNO * CHALLENGE_NUM_PER_ITER * REPEAT_NUM_PER_CHALLENGE) + "_" + convertInt(TUNING_LEVEL) + "_" + convertInt(DEVICE);
		// crp_copytofile(input_storage, output_storage, numOps * ITERNO * INPUT_OUTPUT_BUFFER_RATIO, numOps * ITERNO, filename_crp, circulation);
	
		finish = clock();
		duration = (double)(finish - launch)/(CLOCKS_PER_SEC*60);
		printf("%f time\n" ,duration);
		if(duration <5){
			flag = 1;
		}
		else if(duration > 1440){
			system("PAUSE");
		}
		else{
			flag = 0;
		}

	}
		delete SIRC_P;
		// free(input_storage);
		// free(output_storage);	

		#ifdef BUGHUNT
		    StartLog(susp);
		#endif

	


    PrintZeLog();
	return 0;

}