// Copyright (C) 2009 Sjoerd van der Berg ( harekiet @ gmail.com )

#include "cg_demos.h"

#ifdef RELDEBUG
//#pragma optimize("", off)
#endif

static demoCommandPoint_t* commandPointAlloc(void) {
	demoCommandPoint_t* point = (demoCommandPoint_t*)malloc(sizeof(demoCommandPoint_t));
	if (!point)
		CG_Error("Out of memory");
	memset(point, 0, sizeof(demoCommandPoint_t));
	return point;
}

static void commandPointFree(demoCommandPoint_t* point) {
	if (point->prev)
		point->prev->next = point->next;
	else
		demo.commands.points = point->next;
	if (point->next)
		point->next->prev = point->prev;
	free(point);
}

demoCommandPoint_t* commandPointSynch(int playTime) {
	demoCommandPoint_t* point = demo.commands.points;
	if (!point)
		return 0;
	for (; point->next && point->next->time <= playTime; point = point->next);
	return point;
}

// Get command point for specific layer
demoCommandPoint_t* commandPointSynchForLayer(int playTime,int layer) {
	demoCommandPoint_t* point = demo.commands.points;
	if (!point)
		return 0;
	demoCommandPoint_t* lastValidPoint = point->layer == layer ? point : 0;
	while (1) {
		if (point->next && (point->next->time <= playTime || !lastValidPoint)) {
			point = point->next;
			if (point->layer == layer) {
				lastValidPoint = point;
			}
		}
		else {
			break;
		}
	}
	return lastValidPoint;
}


extern void trap_R_ParseWaveform(const char* text, waveForm_t* wf);
extern float trap_R_EvalWaveform(waveForm_t* wf);

static void evaluateCommandVariable(demoCommandVariable_t* var) {
	var->isValid = qfalse;
	char* text = var->raw;
	if (strlen(text)) {
		if (text[0] == "$") { // Prepending any command variable with $ will make it a hard value that is not interpolated from the previous point that has a value for this variable.
			var->interpolate = qfalse;
			text++;
		}
		else {
			var->interpolate = qtrue;
		}
		if (isdigit(text[0])) {
			var->value = atof(text);
			var->type = DEMO_COMMAND_VARIABLE_VALUE;
			var->isValid = qtrue; // Yeah not very elegant. Would be better to verify somehow. Oh well.
		}
		else {
			char* tmp = text;
			trap_R_ParseWaveform(tmp, &var->waveForm);
			var->type = DEMO_COMMAND_VARIABLE_WAVEFORM;
			var->isValid = qtrue; // Yeah not very elegant. Would be better to verify somehow. Oh well.
		}
	}
}

static qboolean commandPointAdd(int layer, int time, const char* command, demoCommandVariableRawCollection_t* variableCollection) {
	demoCommandPoint_t* point = commandPointSynch(time);
	demoCommandPoint_t* newPoint;
	int i;
	if (!point || point->time > time) {
		newPoint = commandPointAlloc();
		newPoint->next = demo.commands.points;
		if (demo.commands.points)
			demo.commands.points->prev = newPoint;
		demo.commands.points = newPoint;
		newPoint->prev = 0;
	}
	else if (point->time == time) {
		newPoint = point;
	}
	else {
		newPoint = commandPointAlloc();
		newPoint->prev = point;
		newPoint->next = point->next;
		if (point->next)
			point->next->prev = newPoint;
		point->next = newPoint;
	}
	newPoint->time = time;
	strcpy_s(newPoint->command, sizeof(newPoint->command), command);
	newPoint->layer = layer;
	for (int i = 0; i< MAX_DEMO_COMMAND_VARIABLES; i++) {
		strcpy_s(newPoint->variables[i].raw, sizeof(demoCommandVariableRaw_t), (*variableCollection)[i]);
		evaluateCommandVariable(&newPoint->variables[i]);
	}
	return qtrue;
}

static qboolean commandPointDel(int playTime) {
	demoCommandPoint_t* point = commandPointSynch(playTime);

	if (!point || !point->time == playTime)
		return qfalse;
	commandPointFree(point);
	return qtrue;
}

static void commandsClear(void) {
	demoCommandPoint_t* next, * point = demo.commands.points;
	while (point) {
		next = point->next;
		commandPointFree(point);
		point = next;
	}
}

typedef struct {
	int		variableNum;
	char	raw[MAX_DEMO_COMMAND_VARIABLE_LENGTH];
	qboolean hasVariableNum, hasRaw;
} parseCommandPointVariable_t;

typedef struct {
	int layer;
	int time;
	char command[MAX_DEMO_COMMAND_LENGTH];
	demoCommandVariableRawCollection_t variables;
	qboolean variableExists[MAX_DEMO_COMMAND_VARIABLES];
	qboolean hasTime, hasCommand, hasLayer;
} parseCommandPoint_t;

static qboolean commandParseTime(BG_XMLParse_t* parse, const char* line, void* data) {
	parseCommandPoint_t* point = (parseCommandPoint_t*)data;
	if (!line[0])
		return qfalse;
	point->time = atoi(line);
	point->hasTime = qtrue;
	return qtrue;
}
static qboolean commandParseLayer(BG_XMLParse_t* parse, const char* line, void* data) {
	parseCommandPoint_t* point = (parseCommandPoint_t*)data;
	if (!line[0])
		return qfalse;
	point->layer = atoi(line);
	point->hasLayer = qtrue;
	return qtrue;
}
static qboolean commandParseCommand(BG_XMLParse_t* parse, const char* line, void* data) {
	parseCommandPoint_t* point = (parseCommandPoint_t*)data;
	if (!line[0])
		return qfalse;
	strcpy_s(point->command, sizeof(point->command), line);
	point->hasCommand = qtrue;
	return qtrue;
}
static qboolean commandParseRaw(BG_XMLParse_t* parse, const char* line, void* data) {
	parseCommandPointVariable_t* point = (parseCommandPointVariable_t*)data;
	if (!line[0])
		return qfalse;
	strcpy_s(point->raw, sizeof(point->raw), line);
	point->hasRaw = qtrue;
	return qtrue;
}
static qboolean commandParseVariableNum(BG_XMLParse_t* parse, const char* line, void* data) {
	parseCommandPointVariable_t* point = (parseCommandPointVariable_t*)data;
	if (!line[0])
		return qfalse;
	point->variableNum = atoi(line);
	point->hasVariableNum = qtrue;
	return qtrue;
}
static qboolean commandParseVariable(BG_XMLParse_t* parse, const struct BG_XMLParseBlock_s* fromBlock, void* data) {
	parseCommandPoint_t* pointLoad = (parseCommandPoint_t*)data;
	parseCommandPointVariable_t variableLoad;
	static BG_XMLParseBlock_t commandParseBlock[] = {
		{"variableNum", 0, commandParseVariableNum},
		{"raw", 0, commandParseRaw},
		{0, 0, 0}
	};
	memset(&variableLoad, 0, sizeof(variableLoad));
	if (!BG_XMLParse(parse, fromBlock, commandParseBlock, &variableLoad)) {
		return qfalse;
	}
	if (!variableLoad.hasRaw || !variableLoad.hasVariableNum)
		return BG_XMLError(parse, "Missing section in command point variable");
	if (variableLoad.variableNum >= MAX_DEMO_COMMAND_VARIABLES)
		return BG_XMLError(parse, "variableNum in command point variable too high.");

	strcpy_s(pointLoad->variables[variableLoad.variableNum],sizeof(demoCommandVariableRaw_t), variableLoad.raw);
	 
	return qtrue;
}
static qboolean commandParsePoint(BG_XMLParse_t* parse, const struct BG_XMLParseBlock_s* fromBlock, void* data) {
	parseCommandPoint_t pointLoad;
	static BG_XMLParseBlock_t commandParseBlock[] = {
		{"layer", 0, commandParseLayer},
		{"time", 0, commandParseTime},
		{"command", 0, commandParseCommand},
		{"variable", commandParseVariable, 0},
		{0, 0, 0}
	};
	memset(&pointLoad, 0, sizeof(pointLoad));
	if (!BG_XMLParse(parse, fromBlock, commandParseBlock, &pointLoad)) {
		return qfalse;
	}
	if (!pointLoad.hasTime/* || !pointLoad.hasCommand*/) // We allow points without a command, for example for only manipulating variables
		return BG_XMLError(parse, "Missing section in command point");
	if (!pointLoad.hasLayer)
		pointLoad.layer = 0;
	commandPointAdd(pointLoad.layer,pointLoad.time, pointLoad.command,pointLoad.variables);
	return qtrue;
}
static qboolean commandsParseLocked(BG_XMLParse_t* parse, const char* line, void* data) {
	if (!line[0])
		return qfalse;
	demo.commands.locked = atoi(line);
	return qtrue;
}
qboolean commandsParse(BG_XMLParse_t* parse, const struct BG_XMLParseBlock_s* fromBlock, void* data) {
	static BG_XMLParseBlock_t commandsParseBlock[] = {
		{"point",	commandParsePoint,	0},
		{"locked",	0,				commandsParseLocked },

		{0, 0, 0}
	};

	commandsClear();
	if (!BG_XMLParse(parse, fromBlock, commandsParseBlock, data))
		return qfalse;
	return qtrue;
}


void commandsSave(fileHandle_t fileHandle) {
	demoCommandPoint_t* point;
	int i;
	qboolean hasAtLeastOneVariable = qfalse;

	point = demo.commands.points;
	demoSaveLine(fileHandle, "<commands>\n");
	demoSaveLine(fileHandle, "\t<locked>%d</locked>\n", demo.commands.locked);
	while (point) {
		demoSaveLine(fileHandle, "\t<point>\n");
		demoSaveLine(fileHandle, "\t\t<layer>%10d</layer>\n", point->layer);
		demoSaveLine(fileHandle, "\t\t<time>%10d</time>\n", point->time);
		if (strlen(point->command)) {
			demoSaveLine(fileHandle, "\t\t<command>%s</command>\n", point->command);
		}

		for (i = 0; i < MAX_DEMO_COMMAND_VARIABLES; i++) {
			if (!strlen(point->variables[i].raw)) {
				hasAtLeastOneVariable = qtrue;
			}
		}
		if (hasAtLeastOneVariable) {

			for (i = 0; i < MAX_DEMO_COMMAND_VARIABLES; i++) {
				if (strlen(point->variables[i].raw)) {
					demoSaveLine(fileHandle, "\t\t<variable>\n");
					demoSaveLine(fileHandle, "\t\t\t<variableNum>%d</variableNum>\n", i);
					demoSaveLine(fileHandle, "\t\t\t<raw>%s</raw>\n", point->variables[i].raw);
					demoSaveLine(fileHandle, "\t\t</variable>\n");
				}
			}
		}

		demoSaveLine(fileHandle, "\t</point>\n");
		point = point->next;
	}
	demoSaveLine(fileHandle, "</commands>\n");
}

void demoCommandsCommand_f(void) {
	const char* cmd = CG_Argv(1);
	if (!Q_stricmp(cmd, "lock")) {
		demo.commands.locked = !demo.commands.locked;
		if (demo.commands.locked)
			CG_DemosAddLog("Commands locked");
		else
			CG_DemosAddLog("Commands unlocked");
	}
	else if (!Q_stricmp(cmd, "add")) {
		int i;
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
			const char* variabletext = CG_Argv(3+ offset +i);
			int varNum = i;
			if (strlen(variabletext) > 2 && isdigit(variabletext[0]) && variabletext[1] == ':') {
				// A specific varnum was specified
				varNum = variabletext[0] - '0'; // Set correct varnum to write to
				variabletext += 2; // Forward the text pointer by 2 to skip the varnum declaration
			}
			if (strlen(variabletext)) {
				strcpy_s((*varCollection)[varNum],sizeof(demoCommandVariableRaw_t),variabletext);
				hasAtLeastOneVariable = qtrue;
			}
		}
		const char* commandToAdd = CG_Argv(2+ offset);
		if (!strlen(commandToAdd) && !hasAtLeastOneVariable) { // We Will actually allow empty commands now so we can make keypoints solely for the variables

			CG_DemosAddLog("Failed to add command point. Need at least a command or one variable. Syntax: commands add \"[command]\" \"[optional variable 0\"  \"[optional variable 1\"...");
		}
		else {
			if (commandPointAdd(layer,demo.play.time, commandToAdd, varCollection)) {
				CG_DemosAddLog("Added command point");
			}
			else {
				CG_DemosAddLog("Failed to add command point");
			}
		}
		free(varCollection);
	}
	else if (!Q_stricmp(cmd, "del")) {
		if (commandPointDel(demo.play.time)) {
			CG_DemosAddLog("Deleted command point");
		}
		else {
			CG_DemosAddLog("Failed to delete command point");
		}
	}
	else if (!Q_stricmp(cmd, "clear")) {
		commandsClear();
	}
	/*else if (!Q_stricmp(cmd, "set")) {
		cmd = CG_Argv(2);
		if (cmd[0])
			Com_sprintf()
			demo.line.speed = atof(cmd);
		CG_DemosAddLog("Timeline speed %.03f", demo.line.speed);
	}*/
	else if (!Q_stricmp(cmd, "next")) {
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
	}
	else if (!Q_stricmp(cmd, "start")) {
		demo.commands.start = demo.play.time;
		if (demo.commands.start > demo.commands.end)
			demo.commands.start = demo.commands.end;
		if (demo.commands.end == demo.commands.start)
			CG_DemosAddLog("Cleared commands selection");
		else
			CG_DemosAddLog("Commands selection start at %d.%03d", demo.commands.start / 1000, demo.commands.start % 1000);
	}
	else if (!Q_stricmp(cmd, "end")) {
		demo.commands.end = demo.play.time;
		if (demo.commands.end < demo.commands.start)
			demo.commands.end = demo.commands.start;
		if (demo.commands.end == demo.commands.start)
			CG_DemosAddLog("Cleared commands selection");
		else
			CG_DemosAddLog("Commands selection end at %d.%03d", demo.commands.end / 1000, demo.commands.end % 1000);
	}
	else {
		Com_Printf("commands usage:\n");
		Com_Printf("commands lock, lock commands to use the keypoints\n");
		Com_Printf("commands add \"[command]\", Add command point for this point in time\n");
		Com_Printf("commands del, Delete command keypoint\n");
		Com_Printf("commands clear, Clear all keypoints\n");
		Com_Printf("commands start/end, set start/end parts of commands selection\n");
	}
}

void evaluateDemoCommand() {
	int i,l, srcLength;
	qboolean isDynamic = qfalse;
	qboolean varsHaveChanged = qfalse;
	const char skipWriteCmdOn[] = "com_skipWrite 1;";
	const char skipWriteCmdOff[] = ";com_skipWrite 0";
	char composedCommand[MAX_DEMO_COMMAND_LENGTH+sizeof(skipWriteCmdOn)+sizeof(skipWriteCmdOff)];

	for (l = 0; l < MAX_DEMO_COMMAND_LAYERS; l++) {

		if (!demo.commands.locked) {
			demo.commands.lastPoint[l] = 0;
			continue;
		}

		demoCommandPoint_t* cmdHere = commandPointSynchForLayer(demo.play.time,l);
	
		if (!cmdHere) {
			return;
		}

		memset(composedCommand, 0, sizeof(composedCommand));

		char* text = cmdHere->command;

		strcat_s(composedCommand, sizeof(composedCommand), skipWriteCmdOn);// Don't write anything to a config changed during execution of a command point. We would have a config write on every single frame. Bad.

		srcLength = max(strlen(cmdHere->command),sizeof(cmdHere->command));
		for (i = 0; i < srcLength;i++) {
			if (text[i] == '%' && isdigit(text[i + 1]) && (i == 0 || text[i - 1] != '\\')) {
				// This is a variable
				char formattedNumber[100];

				int variableNum = text[i + 1]-'0'; 
				float result;
				if (evaluateCommandVariableAt(variableNum,&result)) {
					isDynamic = qtrue; // Probably get rid of this, it's not reliable anyway.
				}
				if (demo.commands.lastValue[variableNum] != result){
					varsHaveChanged = qtrue;
				}
				demo.commands.lastValue[variableNum] = result;

				sprintf_s(formattedNumber, sizeof(formattedNumber), "%.5f", result);
				strcat_s(composedCommand, sizeof(composedCommand), formattedNumber);

				i++;
			}
			else {
				strncat_s(composedCommand,sizeof(composedCommand),&text[i],1);
			}
		}

		strcat_s(composedCommand, sizeof(composedCommand), skipWriteCmdOff);

		strncat_s(composedCommand, sizeof(composedCommand), "\n", 1);

		if (demo.commands.lastPoint[l] != cmdHere || isDynamic || varsHaveChanged) { // Don't execute a command twice unless it is dynamic

			trap_SendConsoleCommand(composedCommand);
		}
		demo.commands.lastPoint[l] = cmdHere;

	}
}

qboolean evaluateCommandVariableAt(int variableNumber, float* result) {

	qboolean isDynamic = qfalse;
	demoCommandPoint_t* last, *next;
	demoCommandPoint_t* cmdHere = commandPointSynch(demo.play.time);
	if (!cmdHere) {
		*result = 0.0f;
		return;
	}

	// Find *from* keypoint.
	if (cmdHere->time > demo.play.time) { 
		// The keypoint lies in the future. So there's no previous keyframe.
		last = 0;
	}
	else {
		last = cmdHere;
		while (last && !last->variables[variableNumber].isValid) {
			// If the current keypoint doesn't have a value for this variable, keep going into the past.
			last = last->prev;
		}
	}

	if (last && last->time == demo.play.time && demo.play.fraction == 0.0f) {
		// We're exactly on the keypoint right now. No need to interpolate.
		next = 0;
	}
	else {
		// Find *to* keypoint.
		next = cmdHere->next;
		while (next && !next->variables[variableNumber].isValid) {
			next = next->next;
		}
	}
	

	if ((last && !next) || (last && next && next->variables[variableNumber].interpolate == qfalse)) {
		// If there is no next, or if next has disabled interpolation, there is no transition. Use last value.
		return evaluateCommandVariableValue(&last->variables[variableNumber],result);
	}
	else if (next && !last) {
		// There is no transition. Use next value.
		return evaluateCommandVariableValue(&next->variables[variableNumber], result);
	}
	else if (last && next) {
		float lastValue, nextValue;
		evaluateCommandVariableValue(&last->variables[variableNumber], &lastValue);
		evaluateCommandVariableValue(&next->variables[variableNumber], &nextValue);
		// Lerp.
		*result = lastValue + (float)(nextValue - lastValue) * (((float)demo.play.time+demo.play.fraction - (float)last->time) / (float)(next->time - last->time));
		return qtrue;
	}
	else {
		// Bah nonsense
		*result = 0.0f;
		return qfalse; 
	}


	
}

qboolean evaluateCommandVariableValue(demoCommandVariable_t* variable, float* result) {

	switch (variable->type) {
	case DEMO_COMMAND_VARIABLE_VALUE:
		*result = variable->value;
		return qfalse;
	case DEMO_COMMAND_VARIABLE_WAVEFORM:
		*result = trap_R_EvalWaveform(&variable->waveForm);
		return qtrue;
		break;
	default:
		// Shouldn't really happen
		*result = 0.0f;
		return qfalse;
	}
}



#ifdef RELDEBUG
//#pragma optimize("", on)
#endif