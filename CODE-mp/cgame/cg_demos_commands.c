// Copyright (C) 2009 Sjoerd van der Berg ( harekiet @ gmail.com )

#include "cg_demos.h"

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

static qboolean commandPointAdd(int time, const char* command) {
	demoCommandPoint_t* point = commandPointSynch(time);
	demoCommandPoint_t* newPoint;
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
	Com_sprintf(newPoint->command, sizeof(newPoint->command), command);
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
	int time;
	char command[MAX_DEMO_COMMAND_LENGTH];
	qboolean hasTime, hasCommand;
} parseCommandPoint_t;

static qboolean commandParseTime(BG_XMLParse_t* parse, const char* line, void* data) {
	parseCommandPoint_t* point = (parseCommandPoint_t*)data;
	if (!line[0])
		return qfalse;
	point->time = atoi(line);
	point->hasTime = qtrue;
	return qtrue;
}
static qboolean commandParseCommand(BG_XMLParse_t* parse, const char* line, void* data) {
	parseCommandPoint_t* point = (parseCommandPoint_t*)data;
	if (!line[0])
		return qfalse;
	Com_sprintf(point->command, sizeof(point->command), line);
	point->hasCommand = qtrue;
	return qtrue;
}
static qboolean commandParsePoint(BG_XMLParse_t* parse, const struct BG_XMLParseBlock_s* fromBlock, void* data) {
	parseCommandPoint_t pointLoad;
	static BG_XMLParseBlock_t commandParseBlock[] = {
		{"time", 0, commandParseTime},
		{"command", 0, commandParseCommand},
		{0, 0, 0}
	};
	memset(&pointLoad, 0, sizeof(pointLoad));
	if (!BG_XMLParse(parse, fromBlock, commandParseBlock, &pointLoad)) {
		return qfalse;
	}
	if (!pointLoad.hasTime || !pointLoad.hasCommand)
		return BG_XMLError(parse, "Missing section in command point");
	commandPointAdd(pointLoad.time, pointLoad.command);
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

	point = demo.commands.points;
	demoSaveLine(fileHandle, "<commands>\n");
	demoSaveLine(fileHandle, "\t<locked>%d</locked>\n", demo.commands.locked);
	while (point) {
		demoSaveLine(fileHandle, "\t<point>\n");
		demoSaveLine(fileHandle, "\t\t<time>%10d</time>\n", point->time);
		demoSaveLine(fileHandle, "\t\t<command>%s</command>\n", point->command);
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
		const char* commandToAdd = CG_Argv(2);
		if (!strlen(commandToAdd)) {

			CG_DemosAddLog("Failed to add command point. You must specify a command like this: commands add \"[command]\"");
		}
		else {
			if (commandPointAdd(demo.play.time, commandToAdd)) {
				CG_DemosAddLog("Added command point");
			}
			else {
				CG_DemosAddLog("Failed to add command point");
			}
		}
		
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