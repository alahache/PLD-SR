/*************************************************************************
                           ServerSettings  -  description
                             -------------------
    Creation             : 05 Febr. 2012
    Copyright            : (C) 2012 by H4311 - Benjamin PLANCHE (BPE)
*************************************************************************/

//---- Implementation - <ServerSettings> (ServerSettings.cpp file) -----

//---------------------------------------------------------------- INCLUDE

//--------------------------------------------------------- System Include
using namespace std;
#include <unistd.h>
#include <iostream>
#include <exception>
//------------------------------------------------------ Personnal Include
#include "ServerSettings.h"
#include "../Libs/jsoncpp-src-amalgamation0.6.0-rc2/json/json.h"
extern "C" {
	#include "../Bdd/mysqlinsert.h"
}

//-------------------------------------------------------------- Constants

//----------------------------------------------------------------- PUBLIC

//--------------------------------------------------------- Public Methods



void* ServerSettingsThread_Receive (void* param) {
	int n;
	string msg;
	ServerSettings* server = (ServerSettings*)param;

	MYSQL* mysql = connectToMysql();

	// Wait for client :
	while((server->getFlag() == 0) && (server->acceptClient() >= 0)) {

		// Sends data from each sensor, sleeping between two sending :
		while(server->getFlag() == 0) {

			// Waiting for data to be read :
			server->server.waitData();

			if((n = server->server.readClientJSON(&msg)) <= 0)
			{
				cout << "<Server Settings> Error - Read | " << n << " - Message : " << msg << endl;
				break;
			}

			cout << "<Server Settings> Order Received - " << msg << ".\n";

			Json::Value root;   // will contains the root value after parsing.
			Json::Reader reader;
			bool parsingSuccessful = reader.parse( msg, root );
			if ( !parsingSuccessful )
			{
				// report to the user the failure and their locations in the document.
				cout << "<Server Settings> Error - Parsing | Message : " << msg  << " | Error : "<< reader.getFormatedErrorMessages() << endl;
			}
			else {
				try {
					// Get the ID :
					unsigned int idDevice = root.get("i", 0).asUInt();
					// Get the action :
					int idAction = root.get("a", 0).asInt();
						// idAction :	1 -> Add Device			Needed values : ID, type, Subjects/Peoples, Simulated
						//				2 -> Delete Device		Needed values : ID
						//				3 -> Set Actuator		Needed values : ID, type, on/off, value

					if (idAction == 1) {
						int type = root.get("t", 0).asInt();
						const Json::Value arraySubjects = root["s"];
		#ifdef SIMULATION
						bool simulated = root.get("sim", false).asBool();
		#endif
						int typeDevice = (type >> 24); // If the type starts with an "1" or more, it's an actuator ; else it's a sensor.
						if (typeDevice == 0) { // SENSOR
							// Insert into DB :
							insertCapteur(mysql, type, idDevice, arraySubjects.get("t", 0).asInt(), arraySubjects.get("i", 0).asInt());

							// Add to the "driver" :
							pthread_mutex_lock(&(server->mutex));
							EnOceanSensorAPI::EnOceanCallbackFunction translator = EnOceanSensorAPI::getFunctionPerType(type);
							if (translator != NULL) {
								server->sensors->add(idDevice, translator);
								cout << "<Server Settings> Sensor n°" << idDevice << " - Added !\n";
		#ifdef SIMULATION
								if (simulated) {
									Subject* subject = server->simu->findSubject(arraySubjects.asInt());
									server->simu->addSensor(SensorSimulator::createSensorSimulator(idDevice, type, subject));
									cout << "<Server Settings> Simulated Sensor n°" << idDevice << " - Created !\n";
								}
		#endif
							}

							pthread_mutex_unlock(&(server->mutex));
						}
						else if (typeDevice == 1) { // ACTUATOR
							// Add to the DB :
							insertActionneur(mysql, idDevice, type);
							// Add the connexions to the BD :
							for (unsigned int i = 0; i < arraySubjects.size(); ++i ) {
								int idSubject = arraySubjects[i].get("i", 0).asInt();
								int typeSubject = arraySubjects[i].get("t", 0).asInt();
								char desc = '0';
								insertActionneurSujet(mysql, idDevice, &desc, typeSubject, idSubject);
							}

							cout << "<Server Settings> Actuator n°" << idDevice << " - Added !\n";
		#ifdef SIMULATION
							if (simulated) {
								pthread_mutex_lock(&(server->mutex));
								Actuator* simAc = Actuator::createActuator(idDevice, type);
								for (unsigned int i = 0; i < arraySubjects.size(); ++i ) {
									Subject* subject = server->simu->findSubject(arraySubjects[i].asInt());
									if (subject != NULL) {
										simAc->addSubject(subject);
									}
								}

								server->simu->addActuator(simAc);
								cout << "<Server Settings> Simulated Actuator n°" << idDevice << " - Created !\n";
								pthread_mutex_unlock(&(server->mutex));
							}
		#endif
						}
		#ifdef SIMULATION
						else if ((typeDevice == 2) && simulated) { // SIMULATED EVENT
							pthread_mutex_lock(&(server->mutex));
							Actuator* simAc = Actuator::createActuator(idDevice, type);
							for (unsigned int i = 0; i < arraySubjects.size(); ++i ) {
								Subject* subject = server->simu->findSubject(arraySubjects[i].asInt());
								if (subject != NULL) {
									simAc->addSubject(subject);
								}
							}

							server->simu->addActuator(simAc);
							cout << "<Server Settings> Simulated Event n°" << idDevice << " - Created !\n";
							pthread_mutex_unlock(&(server->mutex));
						}
		#endif
					}
					else if (idAction == 2) {
						pthread_mutex_lock(&(server->mutex));
						if (server->sensors->del(idDevice)) { // If we are deleting a sensor :
							// TO DO:
							//		DELETE FROM THE MYSQL DB !
							cout << "<Server Settings> Sensor n°" << idDevice << " - Deleted !\n";
		#ifdef SIMULATION
							if (server->simu->delSensor(idDevice)) {
								cout << "<Server Settings> Simulated Sensor n°" << idDevice << " - Deleted !\n";
							}
		#endif
						}
						else { // If we are deleting an actuator :
							// TO DO:
							//		DELETE FROM THE MYSQL DB !
							cout << "<Server Settings> Actuator n°" << idDevice << " - Deleted !\n";
		#ifdef SIMULATION
							if (server->simu->delActuator(idDevice)) {
								cout << "<Server Settings> Simulated Actuator n°" << idDevice << " - Deleted !\n";
							}
		#endif
						}
						pthread_mutex_unlock(&(server->mutex));
					}
					else if (idAction == 3) {
						int type = root.get("t", 0).asInt();
						bool on = root.get("e", 0).asInt() == 1;
						float value = (float)root.get("v", 0.0).asDouble();
						enocean_data_structure frame;
						EnOceanSensorAPI::toFrame_Actuator(&frame, (EnOceanSensorAPI::SensorId)idDevice, type, on, value);
						char buffer[EnOceanSensorAPI::FRAME_SIZE];
						EnOceanSensorAPI::toString(&frame, buffer);
						bool trueActuator = true;
#ifdef SIMULATION
						trueActuator = !(server->simu->isSimulatedActuator(idDevice));
#endif
						pthread_mutex_lock(&(server->mutex));
						if (trueActuator) {
							server->msgToSend->push(new string(buffer));
						}
						else {
							server->msgToSendSimu->push(new string(buffer));
						}
						pthread_mutex_unlock(&(server->mutex));
					}
					else {
						cout << "<Server Settings> Error - Unknown Service.\n";
					}
				} catch (exception e) {
					 cout << "<Server Settings> Error - Parsing | Message : " << msg  << " | Error : " << e.what() << endl;
				}

			}
		}

		server->closeClient();
	}
	closeMysql(mysql);

	return NULL;
}//----- End of EnOceanBaseSimulatorThread_Send


int ServerSettings::openSocket(int port) {
	return server.openSocket(port);
}

int ServerSettings::acceptClient() {
	return server.acceptClient();
}

int ServerSettings::closeClient() {
	return server.closeClient();

}
int ServerSettings::closeSocket() {
	return server.closeSocket();
}

int ServerSettings::readClient(char* msg, int length) {
	return server.readClient(msg, length);
}

int ServerSettings::getFlag() {
	int f;
	pthread_mutex_lock(&mutex);
	f = flag;
	pthread_mutex_unlock(&mutex);
	return f;
}

void ServerSettings::run() {
	pthread_mutex_lock(&mutex);
	flag = 0;
	pthread_mutex_unlock(&mutex);
	pthread_create(&thread_Receive, NULL, ServerSettingsThread_Receive, this);
}

void ServerSettings::stop() {
	pthread_mutex_lock(&mutex);
	flag = 1;
	pthread_mutex_unlock(&mutex);
	pthread_join(thread_Receive, NULL);
}

//------------------------------------------------- Static public Methods

//------------------------------------------------------------- Operators


//-------------------------------------------------- Builder / Destructor
ServerSettings::ServerSettings(DeviceTable* sens,  blocking_queue<string>* msg
#ifdef SIMULATION
		,  blocking_queue<string>* msgSimu
		, EnOCeanBaseSimulator* sim
#endif
): sensors(sens), msgToSend(msg)
#ifdef SIMULATION
, msgToSendSimu(msgSimu)
, simu(sim)
#endif
{
	pthread_mutex_init(&mutex, NULL);
	flag = 0;
} //----- End of ServerSettings

ServerSettings::~ServerSettings() {
	// TODO Auto-generated destructor stub
	stop();
	pthread_mutex_destroy(&mutex);
} //----- End of ~ServerSettings


//---------------------------------------------------------------- PRIVATE

//------------------------------------------------------ Protected Methods
