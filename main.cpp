#include <ncurses.h>
#include <unistd.h>
#include <stdlib.h>
#include <unordered_set>
#include <stdio.h>
#include <string>
#include <vector>
#include <list>
#include <chrono>
#include <assert.h>

std::vector<std::vector<std::string>> gWords(100);
const char* ws = " \t\n\r\f\v";

// trim from end of string (right)
inline std::string& rtrim(std::string& s, const char* t = ws)
{
	s.erase(s.find_last_not_of(t) + 1);
	return s;
}

// trim from beginning of string (left)
inline std::string& ltrim(std::string& s, const char* t = ws)
{
	s.erase(0, s.find_first_not_of(t));
	return s;
}

// trim from both ends of string (right then left)
inline std::string& trim(std::string& s, const char* t = ws)
{
	return ltrim(rtrim(s, t), t);
}

void drawClipped(WINDOW* win, int y, int x, std::string msg)
{
	int maxy, maxx;
	getmaxyx(win, maxy, maxx);
	int len= msg.size();
	if( x < 0 )
	{
		int leftclip = -x;
		msg = msg.substr(leftclip);
		x = 0;
	}		
	else if ( x+len >= maxx ) 
	{
		int rightclip = x+len - maxx;
		msg = msg.substr(0, len-rightclip);
		//fprintf(stderr, "x=%d, len=%d, maxx=%d, rightclip=%d\n", x,len,maxx,rightclip);
	}
	mvwprintw(win,y,x, "%s", msg.c_str());
}


static bool endsWith(const std::string& str, const std::string& suffix)
{
	if( str.size() < suffix.size() )
		return false;
	
	int i = 0; 
	int suff_size = suffix.size();
	int str_size = str.size();
	for (;  i < suff_size; i++ )
		if( str[str_size-1-i] != suffix[suff_size-1-i] )
			return false;

	return true;
}

void loadWords()
{
	static const int MAXCHAR = 256;
	if( FILE* f = fopen("words.txt", "r"))
	{
		char buff[MAXCHAR];
		while (fgets(buff, MAXCHAR, f) != nullptr)
		{
			std::string s(buff);
			s = trim(s);
			int len = s.size();
			//if( len > gWords.size()-1 ) 
			//	gWords.resize(len+1); 
			gWords[len].push_back(s);
		}
		fclose(f);
	}
}

struct Round
{
	int minLen,maxLen;
	int minSpeed,maxSpeed;  // Per sec.
	int minWords,maxWords;
	float makeDelayMin;
	int levelUpScore;
} gRounds[] = {
	{ 4,   8,  3,  6,  4,    8,  1.0f,  50   }, 
	{ 6,  10,  4,  8,  6,   10,  0.5f,  100  },
	{ 8,  12,  6, 12,  8,   10,  0.25f, 150  },
	{10,  13,  8, 14,  10,  10,  0.1f,  200  },
	{11,  14, 10, 16,  11,  10,  0.05f, 250  },
};
int gLevel=0; 
bool gQuit = 0;

struct Word
{
	int ypos = 0;
	float xpos = 0.f;
	float speed = 1.f;
	std::string word;
	Word(int y,int x,int s, const std::string& w) :ypos(y),xpos(x),speed(s),word(w) {}
};

struct State
{
	State operator*(double a) const { return *this; }
	State operator+(const State& rhs) const { return *this; }

	WINDOW* board = nullptr;
	WINDOW* scoreboard = nullptr;
	std::vector<int> ycache;
	using ActiveWords = std::list<Word>;
	std::list<Word> lines;
	float makeDelayRemaining = 0.f;
	std::string inputStr;
	int score = 0;
	enum EState { eStart,eNewLevel,eGamePlay};
	EState state = eStart;
	float stateDelay = 0;

	void init()
	{
		int maxy, maxx;
		getmaxyx(stdscr, maxy, maxx);
		board = newwin(maxy-1,maxx, 0,0);
		scoreboard = newwin(1,maxy-1,maxx,0);
	}
	void deinit()
	{
		delwin(board);
		delwin(scoreboard);
	}

	int chooseY()
	{
		int maxy,maxx;
		getmaxyx(board, maxy, maxx);
		int y = rand()%maxy;
		if( y >= ycache.size() ) ycache.resize(y);
		for( ; y < maxy; ++y)
		{
			if(ycache[y]<=0)
			{
				ycache[y]++;
				return y;
			}
		}
		return 0;
	}
	int randRangeSafe(int min, int max)
	{
		int range = max-min;
		if(range <= 0) return min;
		return min + rand() % range;
	}

	void makeWords(double t, double dt)
	{
		Round& r = gRounds[gLevel];
		makeDelayRemaining -= dt;
		if( makeDelayRemaining > 0.f )
			return;
		makeDelayRemaining = r.makeDelayMin;

		if( score >= gRounds[gLevel].levelUpScore )
			return;
	   	
		int maxx,maxy;
		getmaxyx(board, maxy, maxx);
		
		// new a new word?
		if(lines.size() < r.minWords)
		{
			int speed   = randRangeSafe(r.minSpeed, r.maxSpeed);
			int wordLen = randRangeSafe(r.minLen,	r.maxLen);
			int rndWord = rand() % gWords[wordLen].size()-1;
			const std::string& word = gWords[wordLen][rndWord];
			Word w( chooseY(), 
				static_cast<int>(maxx-1 + word.size()), 
				speed, 
				word );
			lines.push_back(w);
		}
	}
	void render()
	{
		clear();

		int maxy,maxx;
		getmaxyx(board, maxy, maxx);
		switch(state)
		{
			default: break;
			case eNewLevel:
			{
				if( gLevel != 0 )
					mvwprintw(board,(maxy/2)-1, (maxx/2)-7, "CONGRATULATIONS" );

				mvwprintw(board,maxy/2, (maxx/2)-4, "LEVEL %d", gLevel+1);
				break;
			}
			case eGamePlay:
			{
				for(auto& i : lines)
				{
					drawClipped(board,i.ypos, (int)i.xpos, i.word);
				}
				mvwprintw(scoreboard,0, 0, "Score: %d, Level: %d", score, gLevel+1);
				break;
			}
		}
		wrefresh(board);
		wrefresh(scoreboard);
	}
	void moveWords(double t, double dt)
	{
		for( ActiveWords::iterator i = lines.begin(); i != lines.end(); )
		{
			float deltaSpeed = (float)i->speed * dt;
			i->xpos -= deltaSpeed;
			if(i->xpos <= -((float)i->word.size()) ) 
			{
				assert(ycache[i->ypos] > 0);
				ycache[i->ypos]--;
				i=lines.erase(i);
			}
			else 
				i++;
		}
	}

	void checkInputAgainstBoard()
	{
		for( ActiveWords::iterator i = lines.begin(); i != lines.end(); )
		{
			if( endsWith(inputStr, i->word))
			{
				ycache[i->ypos]--;
				i=lines.erase(i);
				inputStr.clear();
				score += i->word.size()-1;
			}
			else 
				i++;
		}
	}

	void handleInput()
	{
		char c = getch();
		if( (c >= 'a' && c <= 'z') ||
		    (c >= 'A' && c <= 'Z') || 
		    c == '\'' || c == '-' || c == '.' 
		    )
		{
			inputStr.push_back(c);
			checkInputAgainstBoard();
		}
		else if( c == 27 ) 
		{
			gQuit = true;
			endwin();
			exit(0);
		}
	}

	void tick(double t, double dt)
	{
		switch(state)
		{
			case eStart:
			{
				state = eNewLevel;
				stateDelay = 3.0f;
			}
			case eNewLevel:
			{
				if(stateDelay>0.0f)
				{
					stateDelay -= dt;
					break;
				}
				state=eGamePlay;
			}
			case eGamePlay:
			{
				handleInput();
				makeWords(t, dt);
				moveWords(t, dt);

				if( score >= gRounds[gLevel].levelUpScore &&lines.size() == 0)  
				{
					gLevel++;
					state = eNewLevel;	
					stateDelay = 3.0f;
				}
				break;
			}
		}
	}
}gBoard;

void mainloop()
{
	double t = 0.0;
	double dt = 1.0/30.0;
	auto currentTime = std::chrono::high_resolution_clock::now();
	double accumulator = 0.0;

	//State previousState;
	//State currentState;

	while ( !gQuit )
	{
		auto newTime = std::chrono::high_resolution_clock::now();
   		std::chrono::duration<double,std::chrono::seconds::period> frameTime = newTime - currentTime;
		if ( frameTime.count() > 0.25 )
			frameTime =  std::chrono::duration<double,std::chrono::seconds::period>(0.25);

		currentTime = newTime;
		accumulator += frameTime.count();

		while ( accumulator >= dt )
		{
			//previousState = currentState;
			//currentState.tick(t, dt );
			gBoard.tick(t,dt);
			t += dt;
			accumulator -= dt;
		}

		const double alpha = accumulator / dt;
		//State state = currentState * alpha + previousState * ( 1.0 - alpha );
		//currentState.render();
		//state.render();
		gBoard.render();
	}
}


int main(int argc, char** argv)
{	
	loadWords();
	
	initscr();
#if 0
	noecho();
	int maxy,maxx;
	getmaxyx(stdscr, maxy, maxx);
	for(int i = 0; i < 50; ++i)
	{
		drawClipped(stdscr, 0, maxx-i, "hello world");
		wrefresh(stdscr);
		getch();
		clear();
	}
	endwin();
	return 0;
#endif 

	cbreak(); 
	nodelay(stdscr,TRUE);
	noecho();
	nonl();
	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);
	curs_set(FALSE);

	auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
	srand(seed);

	gBoard.init();
	mainloop();
	
	getch();
	gBoard.deinit();
	endwin();

	return 0;
} 

