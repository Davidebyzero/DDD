/// Private enum for the actions we can perform.
enum Action
{
	UP,
	RIGHT,
	DOWN,
	LEFT,
	NONE,
	
	ACTION_FIRST=UP,
	ACTION_LAST =LEFT
};

inline Action operator++(Action &rs, int) {return rs = (Action)(rs + 1);}
const char* actionNames[] = {"Up", "Right", "Down", "Left", "None"};

const char DX[4] = {0, 1, 0, -1};
const char DY[4] = {-1, 0, 1, 0};

/// Our private level data.
#define X 15
#define Y 15

const char level[Y][X+1] = {
"###############",
"#S#         # #",
"# ##### ### # #",
"#     #   #   #",
"#####   # # # #",
"#     # ### # #",
"# ### # #   # #",
"# # ### ##### #",
"# #   # #     #",
"### # ### #####",
"#S# #     #   #",
"# # # # ### # #",
"# # # # #   # #",
"#   # #   # #F#",
"###############",
};

/// Absolute upper bound. Dictates sizes of some arrays. Mind data types (FRAME, PACKED_FRAME and FRAME_GROUP).
#define MAX_FRAMES 100
#define MAX_STEPS MAX_FRAMES

/// The state as it will be saved on disk. If frame grouping is used, this structure must also contain the subframe field, aligned to byte boundary.
/// Warning: binary comparison is used for this structure, mind alignment gaps and uninitialized data.
struct CompressedState
{
	uint16_t x;
	uint16_t y;

	/// Used for debugging...
	const char* toString() const
	{
		return format("%2d,%2d", x, y);
	}
};

/// Number of bits of data in CompressedState, *excluding* subframe. This is used for efficient comparison operators.
#define COMPRESSED_BITS 32

/// The in-memory structure representing the problem state. Needs to contain at least the functions isFinish, compress, decompress and toString.
struct State
{
	int x, y;

	/// Private method, used in replayStep and expandChildren below.
	/// Returns frame delay, 0 if move is invalid and the state was altered, -1 if move is invalid and the state was not altered.
	int perform(Action action)
	{
		assert (action <= ACTION_LAST);
		if (level[y+DY[action]][x+DX[action]] == '#')
			return -1;
		x += DX[action];
		y += DY[action];
		return 1;
	}

	/// Returns "true" if this is a finish node.
	INLINE bool isFinish() const
	{
		return level[y][x] == 'F';
	}

	/// Compress this instance to CompressedState.
	void compress(CompressedState* s) const
	{
		s->x = x;
		s->y = y;
	}

	/// Restore (overwrite) this instance from CompressedState.
	void decompress(const CompressedState* s)
	{
	    x = s->x;
	    y = s->y;
	}

	/// Return a textual visualisation of the state.
	const char* toString() const
	{
		static char levelstr[Y*(X+1)+1];
		levelstr[Y*(X+1)] = 0;
		for (int sy=0; sy<Y; sy++)
		{
			for (int sx=0; sx<X; sx++)
				levelstr[sy*(X+1) + sx] = level[sy][sx];
			levelstr[sy*(X+1)+X ] = '\n';
		}
		levelstr[y*(X+1) + x] = '@';
		return levelstr;
	}
};

/// A State equality operator is required.
INLINE bool operator==(const State& a, const State& b)
{
	return memcmp(&a, &b, sizeof (State))==0;
}

/// Allows the problem to provide an optimization.
/// Currently, this is used for path backtracking when the solution is found:
/// The function can return "false" to avoid unpacking the parent state and
/// checking whether any of their expansion is the child state.
INLINE bool canStatesBeParentAndChild(const CompressedState* parent, const CompressedState* child)
{
	return true;
}

// ******************************************************************************************************

/// Defines a move within the problem state graph. Doesn't need to be memory-efficient.
//#pragma pack(1)
struct Step
{
	Action action;

	const char* toString()
	{
		return format("%s", actionNames[action]);
	}
};

/// Private function, used in writeSolution below.
void replayStep(State* state, FRAME* frame, Step step)
{
	int res = state->perform((Action)step.action);
	assert(res>0, "Replay failed");
	*frame += res;
}

// ******************************************************************************************************

/// Templated function to enumerate a state's children.
/// Children are collected via one of a few static functions in the template parameter class:
/// static void handleChild(const State* parent, FRAME parentFrame, Step step, const State* state       , FRAME frame)
/// static void handleChild(const State* parent, FRAME parentFrame, Step step, const CompressedState* cs, FRAME frame)
template <class CHILD_HANDLER>
void expandChildren(FRAME frame, const State* state)
{
	State newState = *state;
	for (Action action = ACTION_FIRST; action <= ACTION_LAST; action++)
	{
		int res = newState.perform(action);
		Step step;
		if (res > 0)
		{
			step.action = action;
			CHILD_HANDLER::handleChild(state, frame, step, &newState, frame + res);
		}
		if (res >= 0)
			newState = *state;
	}
}

// ******************************************************************************************************

/// Specifies file name layout used for data files.
const char* formatProblemFileName(const char* name, const char* detail, const char* ext)
{
	return format("%s%s%s.%s", name ? name : "", (name && detail) ? "-" : "", detail ? detail : "", ext);
}

// ******************************************************************************************************

/// Called with the initial state and following steps that lead from it until a finish state. Use to save the solution.
void writeSolution(const State* initialState, Step steps[], int stepNr)
{
	FILE* f = fopen(formatProblemFileName("solution", NULL, "txt"), "wt");
	steps[stepNr].action = NONE;
	State state = *initialState;
	FRAME frame = 0;
	while (stepNr)
	{
		fprintf(f, "%s\n", steps[stepNr].toString());
		fprintf(f, "%s", state.toString());
		replayStep(&state, &frame, steps[--stepNr]);
	}
	// last one
	fprintf(f, "%s\n%s", steps[0].toString(), state.toString());
	fclose(f);
}

// ******************************************************************************************************

#define MAX_INITIAL_STATES 4

/// These set the initial states used to populate frame 0.
State initialStates[MAX_INITIAL_STATES];
int initialStateCount = 0;

/// Problem initialization function.
void initProblem()
{
	printf("SampleMaze: %ux%u\n", X, Y);

	for (int y=0; y<Y; y++)
		for (int x=0; x<X; x++)
			if (level[y][x] == 'S')
			{
				initialStates[initialStateCount].x = x;
				initialStates[initialStateCount].y = y;
				initialStateCount++;
			}
}
