#include "cg_demos.h"

#ifdef RELDEBUG
#pragma optimize("", off)
#endif

void drawDemoObjects() {
	

	demoObject_t* object = demo.objects.objects;

	while (object) {
		if (object->timeIn <= demo.play.time && (!object->timeOut || demo.play.time < object->timeOut)) {

			trap_R_AddPolyToScene(object->shader, 4, object->verts);
		}
		object = object->next;
	}

}

typedef struct {
	char					param1[MAX_DEMO_OBJECT_PARAM_LENGTH]; // For polys: Shader name. For models (not implemented), this could be model name.
	float					size1; // Width for polys. For models (not implemented), this could be scaling factor.
	float					size2; // Height
	vec3_t					angles;
	vec3_t					origin;
	int						timeIn;		// At what time does this start? demotime.
	int						timeOut;	// At what time does this end? 0 = never, >0 = demotime.
	qboolean				hasParam1, hasSize1, hasSize2, hasAngles, hasOrigin, hasTimeIn, hasTimeOut;
} parseObjectPoint_t;

static demoObject_t* objectAlloc(void) {
	demoObject_t* point = (demoObject_t*)malloc(sizeof(demoObject_t));
	if (!point)
		CG_Error("Out of memory");
	memset(point, 0, sizeof(demoObject_t));
	return point;
}

static void objectFree(demoObject_t* point) {
	if (point->prev)
		point->prev->next = point->next;
	else
		demo.objects.objects = point->next;
	if (point->next)
		point->next->prev = point->prev;
	free(point);
}
static void objectsClear(void) {
	demoObject_t* next, * point = demo.objects.objects;
	while (point) {
		next = point->next;
		objectFree(point);
		point = next;
	}
}

static void objectInitCalculations(demoObject_t* object) {

	vec3_t tmp;

	AnglesToAxis(object->angles, object->axes);

	object->shader = trap_R_RegisterShader(object->param1);

	VectorCopy(object->origin, object->verts[0].xyz);
	VectorCopy(object->origin, object->verts[1].xyz);
	VectorCopy(object->origin, object->verts[2].xyz);
	VectorCopy(object->origin, object->verts[3].xyz);

	for (int i = 0; i < 4; i++) {
		object->verts[i].modulate[0] = object->verts[i].modulate[1] = object->verts[i].modulate[2] = object->verts[i].modulate[3] = 255.0f;
	}

	VectorScale(object->axes[1],-object->size1/2.0f, tmp);
	VectorAdd(object->verts[0].xyz, tmp, object->verts[0].xyz);
	VectorScale(object->axes[2],object->size2/2.0f, tmp);
	VectorAdd(object->verts[0].xyz, tmp, object->verts[0].xyz);
	object->verts[0].st[0] = 1.0f;
	object->verts[0].st[1] = 0.0f;

	VectorScale(object->axes[1], -object->size1 / 2.0f, tmp);
	VectorAdd(object->verts[1].xyz, tmp, object->verts[1].xyz);
	VectorScale(object->axes[2], -object->size2 / 2.0f, tmp);
	VectorAdd(object->verts[1].xyz, tmp, object->verts[1].xyz);
	object->verts[1].st[0] = 1.0f;
	object->verts[1].st[1] = 1.0f;

	VectorScale(object->axes[1], object->size1 / 2.0f, tmp);
	VectorAdd(object->verts[2].xyz, tmp, object->verts[2].xyz);
	VectorScale(object->axes[2], -object->size2 / 2.0f, tmp);
	VectorAdd(object->verts[2].xyz, tmp, object->verts[2].xyz);
	object->verts[2].st[0] = 0.0f;
	object->verts[2].st[1] = 1.0f;

	VectorScale(object->axes[1], object->size1 / 2.0f, tmp);
	VectorAdd(object->verts[3].xyz, tmp, object->verts[3].xyz);
	VectorScale(object->axes[2], object->size2 / 2.0f, tmp);
	VectorAdd(object->verts[3].xyz, tmp, object->verts[3].xyz);
	object->verts[3].st[0] = 0.0f;
	object->verts[3].st[1] = 0.0f;



}

demoObject_t* closestObject(vec3_t origin) {
	vec3_t tmp;
	float smallestDistance = INFINITY;
	demoObject_t* object = demo.objects.objects;
	demoObject_t* closestObject = 0;
	while (object) {
		float distance = VectorLength(origin, object->origin);
		if (distance < smallestDistance) {
			smallestDistance = distance;
			closestObject = object;
		}
		object = object->next;
	}
	return closestObject;
}

static demoObject_t* objectAdd(vec3_t origin, vec3_t angles, const char* param1, float size1, float size2, int timeIn, int timeOut) {

	demoObject_t* object = demo.objects.objects;
	demoObject_t* newObject;

	// Just append it at the end. Order doesn't matter with objects.
	newObject = objectAlloc();
	while (object && object->next) {
		object = object->next;
	}
	if (object) {
		object->next = newObject;
		newObject->prev = object;
	}
	else {
		demo.objects.objects = newObject;
	}

	// Copy over values
	VectorCopy(origin, newObject->origin);
	VectorCopy(angles, newObject->angles);
	strcpy_s(newObject->param1,sizeof(newObject->param1),param1);
	newObject->size1 = size1;
	newObject->size2 = size2;
	newObject->timeIn = timeIn;
	newObject->timeOut = timeOut;

	objectInitCalculations(newObject);

	return newObject;
}
void objectsSave(fileHandle_t fileHandle) {
	demoObject_t* point;
	int i;

	point = demo.objects.objects;
	demoSaveLine(fileHandle, "<objects>\n");
	demoSaveLine(fileHandle, "\t<locked>%d</locked>\n", demo.commands.locked);
	while (point) {
		demoSaveLine(fileHandle, "\t<point>\n");
		if(point->timeIn)
			demoSaveLine(fileHandle, "\t\t<timeIn>%10d</timeIn>\n", point->timeIn);
		if (point->timeOut)
			demoSaveLine(fileHandle, "\t\t<timeOut>%10d</timeOut>\n", point->timeOut);
		demoSaveLine(fileHandle, "\t\t<origin>%9.4f %9.4f %9.4f</origin>\n", point->origin[0], point->origin[1], point->origin[2]);
		demoSaveLine(fileHandle, "\t\t<angles>%9.4f %9.4f %9.4f</angles>\n", point->angles[0], point->angles[1], point->angles[2]);
		demoSaveLine(fileHandle, "\t\t<param1>%s</param1>\n", point->param1);
		demoSaveLine(fileHandle, "\t\t<size1>%f</size1>\n", point->size1);
		demoSaveLine(fileHandle, "\t\t<size2>%f</size2>\n", point->size2);
		demoSaveLine(fileHandle, "\t</point>\n");
		point = point->next;
	}
	demoSaveLine(fileHandle, "</objects>\n");
}
static qboolean objectParseParam1(BG_XMLParse_t* parse, const char* line, void* data) {
	parseObjectPoint_t* point = (parseObjectPoint_t*)data;
	if (!line[0])
		return qfalse;
	strcpy_s(point->param1, sizeof(point->param1), line);
	point->hasParam1 = qtrue;
	return qtrue;
}
static qboolean objectParseTimeIn(BG_XMLParse_t* parse, const char* line, void* data) {
	parseObjectPoint_t* point = (parseObjectPoint_t*)data;

	point->timeIn = atoi(line);
	point->hasTimeIn = qtrue;
	return qtrue;
}
static qboolean objectParseTimeOut(BG_XMLParse_t* parse, const char* line, void* data) {
	parseObjectPoint_t* point = (parseObjectPoint_t*)data;

	point->timeOut = atoi(line);
	point->hasTimeOut = qtrue;
	return qtrue;
}
static qboolean objectParseSize1(BG_XMLParse_t* parse, const char* line, void* data) {
	parseObjectPoint_t* point = (parseObjectPoint_t*)data;

	point->size1 = atof(line);
	point->hasSize1 = qtrue;
	return qtrue;
}
static qboolean objectParseSize2(BG_XMLParse_t* parse, const char* line, void* data) {
	parseObjectPoint_t* point = (parseObjectPoint_t*)data;

	point->size2 = atof(line);
	point->hasSize2 = qtrue;
	return qtrue;
}
static qboolean objectParseAngles(BG_XMLParse_t* parse, const char* line, void* data) {
	parseObjectPoint_t* point = (parseObjectPoint_t*)data;

	sscanf(line, "%f %f %f", &point->angles[0], &point->angles[1], &point->angles[2]);
	point->hasAngles = qtrue;
	return qtrue;
}
static qboolean objectParseOrigin(BG_XMLParse_t* parse, const char* line, void* data) {
	parseObjectPoint_t* point = (parseObjectPoint_t*)data;

	sscanf(line, "%f %f %f", &point->origin[0], &point->origin[1], &point->origin[2]);
	point->hasOrigin = qtrue;
	return qtrue;
}
static qboolean objectParsePoint(BG_XMLParse_t* parse, const struct BG_XMLParseBlock_s* fromBlock, void* data) {
	parseObjectPoint_t pointLoad;
	demoObject_t* point;
	static BG_XMLParseBlock_t objectParseBlock[] = {
		{"origin", 0, objectParseOrigin},
		{"angles", 0, objectParseAngles},
		{"timeIn", 0, objectParseTimeIn},
		{"timeOut", 0, objectParseTimeOut},
		{"size1", 0, objectParseSize1},
		{"size2", 0, objectParseSize2},
		{"param1", 0, objectParseParam1},
		{0, 0, 0}
	};
	memset(&pointLoad, 0, sizeof(pointLoad));
	if (!BG_XMLParse(parse, fromBlock, objectParseBlock, &pointLoad)) {
		return qfalse;
	}
	if (!pointLoad.hasAngles || !pointLoad.hasOrigin || !pointLoad.hasParam1 || !pointLoad.hasSize1 || !pointLoad.hasSize2)
		return BG_XMLError(parse, "Missing section in object point variable");

	point = objectAdd(pointLoad.origin,pointLoad.angles,pointLoad.param1,pointLoad.size1,pointLoad.size2,pointLoad.timeIn,pointLoad.timeOut);
	return qtrue;
}
static qboolean objectParseLocked(BG_XMLParse_t* parse, const char* line, void* data) {
	demo.objects.locked = atoi(line);
	return qtrue;
}
qboolean objectsParse(BG_XMLParse_t* parse, const struct BG_XMLParseBlock_s* fromBlock, void* data) {
	static BG_XMLParseBlock_t objectParseBlock[] = {
		{"point",	objectParsePoint,	0 },
		{"locked",	0,					objectParseLocked },
		{0, 0, 0}
	};

	objectsClear();
	if (!BG_XMLParse(parse, fromBlock, objectParseBlock, data))
		return qfalse;

	return qtrue;
}

void demoObjectsCommand_f(void) {
	const char* cmd = CG_Argv(1);
	if (!Q_stricmp(cmd, "lock")) {
		demo.objects.locked = !demo.objects.locked;
		if (demo.objects.locked)
			CG_DemosAddLog("Objects locked");
		else
			CG_DemosAddLog("Objects unlocked");
	}
	else if (!Q_stricmp(cmd, "add")) {

		float size1 = atof(CG_Argv(3));
		float size2 = atof(CG_Argv(4));
		char* param1 = CG_Argv(2);

		objectAdd(demo.viewOrigin,demo.viewAngles, param1,size1,size2,demo.play.time,0);
		/*int i;
		qboolean hasAtLeastOneVariable = qfalse;
		int layer = 0;
		int offset = 0;

		const char* layerOrCommand = CG_Argv(2);
		if (strlen(layerOrCommand) == 1 && isdigit(layerOrCommand[0])) {
			// If we want to, we can first specify the layer and then the command.
			layer = layerOrCommand[0] - '0';
			offset++;
		}

		demoCommandVariableRawCollection_t* varCollection = (demoCommandVariableRawCollection_t*)malloc(sizeof(demoCommandVariableRawCollection_t));
		memset(varCollection, 0, sizeof(demoCommandVariableRawCollection_t));

		for (int i = 0; i < MAX_DEMO_COMMAND_VARIABLES; i++) {
			const char* variabletext = CG_Argv(3 + offset + i);
			int varNum = i;
			if (strlen(variabletext) > 2 && isdigit(variabletext[0]) && variabletext[1] == ':') {
				// A specific varnum was specified
				varNum = variabletext[0] - '0'; // Set correct varnum to write to
				variabletext += 2; // Forward the text pointer by 2 to skip the varnum declaration
			}
			if (strlen(variabletext)) {
				strcpy_s((*varCollection)[varNum], sizeof(demoCommandVariableRaw_t), variabletext);
				hasAtLeastOneVariable = qtrue;
			}
		}
		const char* commandToAdd = CG_Argv(2 + offset);
		if (!strlen(commandToAdd) && !hasAtLeastOneVariable) { // We Will actually allow empty commands now so we can make keypoints solely for the variables

			CG_DemosAddLog("Failed to add command point. Need at least a command or one variable. Syntax: commands add \"[command]\" \"[optional variable 0\"  \"[optional variable 1\"...");
		}
		else {
			if (commandPointAdd(layer, demo.play.time, commandToAdd, varCollection)) {
				CG_DemosAddLog("Added command point");
			}
			else {
				CG_DemosAddLog("Failed to add command point");
			}
		}
		free(varCollection);*/
	}
	else if (!Q_stricmp(cmd, "del")) {
		demoObject_t* closestObj = closestObject(demo.viewOrigin);
		if (closestObj) {
			objectFree(closestObj);
			CG_DemosAddLog("Deleted object");
		}
		else {
			CG_DemosAddLog("Failed to delete object. None found.");
		}
	}
	else if (!Q_stricmp(cmd, "clear")) {
		objectsClear();
	}
	/*else if (!Q_stricmp(cmd, "set")) {
		cmd = CG_Argv(2);
		if (cmd[0])
			Com_sprintf()
			demo.line.speed = atof(cmd);
		CG_DemosAddLog("Timeline speed %.03f", demo.line.speed);
	}*/
	/*else if (!Q_stricmp(cmd, "next")) {
		demoCommandPoint_t* point = commandPointSynch(demo.play.time);
		if (!point)
			return;
		if (point->next)
			point = point->next;
		demo.play.time = point->time;
		demo.play.fraction = 0;
	}
	else if (!Q_stricmp(cmd, "prev")) {
		demoCommandPoint_t* point = commandPointSynch(demo.play.time);
		if (!point)
			return;
		if (point->prev)
			point = point->prev;
		demo.play.time = point->time;
		demo.play.fraction = 0;
	}*/
	else if (!Q_stricmp(cmd, "start")) {
		demoObject_t* closestObj = closestObject(demo.viewOrigin);
		closestObj->timeIn = demo.play.time;
	}
	else if (!Q_stricmp(cmd, "end")) {
		demoObject_t* closestObj = closestObject(demo.viewOrigin);
		closestObj->timeOut = demo.play.time;
	}
	else {
		Com_Printf("objects usage:\n");
		Com_Printf("objects lock, lock commands to use the keypoints\n");
		Com_Printf("objects add \"[shader]\" [width] [height], add object at current location\n");
		Com_Printf("objects del, Delete nearest object\n");
		Com_Printf("objects clear, Clear all objects\n");
		Com_Printf("objects start/end, set timeIn/timeOut of nearest object\n");
	}
}


#ifdef RELDEBUG
#pragma optimize("", on)
#endif