var modelsensors = require("./sensors");
var modelpatients = require("./patients");

function error(code, resp) {
	var result = {};
	result.error = {};
	result.error.code = code;
	
	switch(code) {
		case 0:
			result.error.msg = "Couldn't parse the JSON";
			break;
		default:
			result.error.msg = "Unknow error";
	}
	
	var jsonResult = JSON.stringify(result);
	resp.end(jsonResult);
}

// Adds the header indicating all went sucessfully.
function writeHeaders(resp) {
	resp.writeHead(200, {"Content-type" : "application/json", 
			     "Access-Control-Allow-Origin":"*"});
}

/*
 * SERVICE Sensors
 * Gets records from sensors, allows to specify an interval.
 */
function serviceSensors(query, post, resp) {
	writeHeaders(resp);
	
	// Parse the json request in POST data :
	request = JSON.parse(post);
	if(!request) {
		error(0, resp);
		return;
	}
	
	// Get the response from the modelsensors layer :
	modelsensors.getSensorsRecords(request, function(response) {
		// Send the stringified json to client :
		var strResponse = JSON.stringify(response);
		resp.end(strResponse);
	});
}

/*
 * SERVICE Actuators
 * Allows to pass values to actuators.
 */
function serviceActuators(query, post, resp) {
	writeHeaders(resp);
	
	// Parse the json POST request
	request = JSON.parse(post);
	if(!request) {
		error(0, resp);
		return;
	}
	
	var result = {};
	
	var strResult = JSON.stringify(result);
	resp.end(strResult);
}

/*
 * SERVICE Admin
 * Allows to add/remove sensors/actuators.
 */
function serviceAdmin(query, post, resp) {
	writeHeaders(resp);
	
	// Parse the json POST request
	request = JSON.parse(post);
	if(!request) {
		error(0, resp);
		return;
	}
	
	var result = {};
	
	var strResult = JSON.stringify(result);
	resp.end(strResult);
}

/*
 * SERVICE Patients
 * Allows to add/remove sensors/actuators.
 */
function servicePatients(query, post, resp) {
	writeHeaders(resp);
	
	// Parse the json POST request
	request = JSON.parse(post);
	if(!request) {
		error(0, resp);
		return;
	}	
	
	modelpatients.getPatients(request, function(result) {
			var strResult = JSON.stringify(result);
			resp.end(strResult);
	});
}

exports.sensors = serviceSensors;
exports.actuators = serviceActuators;
exports.patients = servicePatients;
