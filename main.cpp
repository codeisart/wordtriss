#include <ncurses.h>
#include <unistd.h>
#include <stdlib.h>
#include <unordered_set>
#include <stdio.h>
#include <string>
#include <vector>
#include <list>
#include <chrono>

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
} gRounds[] = {
	{4,8,6,7,4,8,1}, 
	{2,6,1,3,5,10,0.5}
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

	std::vector<int> ycache;
	using ActiveWords = std::list<Word>;
	std::list<Word> lines;
	float makeDelayRemaining = 0.f;
	std::string inputStr;

	int chooseY()
	{
		int maxy,maxx;
		getmaxyx(stdscr, maxy, maxx);
		int y = rand()%maxy;
		if( y < ycache.size()-1 ) ycache.resize(y+1);
		for( ; y < maxy; ++y)
		{
			if(!ycache[y])
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
		int maxy, maxx;
		makeDelayRemaining -= dt;
		if( makeDelayRemaining > 0.f )
			return;
		makeDelayRemaining = r.makeDelayMin;

		getmaxyx(stdscr, maxy, maxx);
	
		// new a new word?
		if(lines.size() < r.minWords)
		{
			int speed   = randRangeSafe(r.minSpeed, r.maxSpeed);
			int wordLen = randRangeSafe(r.minLen,	r.maxLen);
			int rndWord = rand() % gWords[wordLen].size()-1;
			const std::string& word = gWords[wordLen][rndWord];
			Word w( chooseY(), 
				static_cast<int>(maxx + word.size()), 
				speed, 
				word );
			lines.push_back(w);
		}
	}
	void render()
	{
		clear();
		for(auto& i : lines)
		{
			 mvprintw(i.ypos, (int)i.xpos, "%s", i.word.c_str());
		}
		refresh();
	}
	void moveWords(double t, double dt)
	{
		for( ActiveWords::iterator i = lines.begin(); i != lines.end(); )
		{
			float deltaSpeed = (float)i->speed * dt;
			i->xpos -= deltaSpeed;
			if(i->xpos <= -((float)i->word.size()) ) 
			{
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
		
			}
			else 
				i++;
		}
	}

	void handleInput()
	{
		char c = getch();
		if( (c >= 'a' && c <= 'z') ||
		    (c >= 'A' && c <= 'Z') )
		{
			inputStr.push_back(c);
			checkInputAgainstBoard();
		}
		else if( c == 27 ) 
		{
			gQuit = true;
			endwin();
			printf("%s\n",inputStr.c_str());
			exit(0);
		}
	}

	void tick(double t, double dt)
	{
		handleInput();

		// make new words.
		makeWords(t, dt);
		// move words
		moveWords(t, dt);
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
	//bool quit = false;

	while ( !gQuit )
	{
		auto newTime = std::chrono::high_resolution_clock::now();
   		std::chrono::duration<double,std::chrono::seconds::period> frameTime = newTime - currentTime;
		if ( frameTime.count() > 0.25 )
			frameTime =  std::chrono::duration<double,std::chrono::seconds::period>(0.25);

		currentTime = newTime;
		accumulator += frameTime.count();

		//printf("t=%f\n", t);
		
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
	cbreak(); 
	nodelay(stdscr,TRUE);
	noecho();
	nonl();
	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);
	curs_set(FALSE);

	auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
	srand(seed);

	mainloop();
	
	getch();
	endwin();

	return 0;
} 

