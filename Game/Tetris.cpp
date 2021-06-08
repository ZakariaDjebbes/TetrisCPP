#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

class Tetris : public olc::PixelGameEngine {
private:
	//Possible tetromino rotations
	enum class Rotation {
		Rotation_0,
		Rotation_90,
		Rotation_180,
		Rotation_270
	};

	enum class GameState {
		Paused,
		Running,
		Over
	};

	struct TetroMino {
		bool* shape; // 4 * 4 bool array represting all a tetromino shape where 1 is filled and 0 is empty
		olc::Pixel color; // color of the shape

		TetroMino(olc::Pixel color, bool shape[])
		{
			this->color = color;
			this->shape = shape;
		}

		TetroMino()
		{
			shape = nullptr;
		}

		~TetroMino()
		{
			delete shape;
		}
	};

	struct Cell {
		bool isAlive; //Is currently a part of the peice controlled by the player
		bool isFilled; //Is currently a solid block on the field
		bool isLine; //Is current a part of a line?
		bool isPreview; //Is currently a part of the preview?
		olc::Pixel fillColor;
		olc::vi2d position; //Position on screen
		olc::vi2d size; //Size in pixels

		Cell()
		{
			isAlive = false;
			isFilled = false;
			isLine = false;
			isPreview = false;
			fillColor = olc::BLANK;
		}

		Cell(olc::Pixel fillColor, bool isFilled, olc::vi2d pos, olc::vi2d size)
		{
			this->fillColor = fillColor;
			this->isFilled = isFilled;
			this->position = pos;
			this->size = size;
			this->isLine = false;
			this->isAlive = false;
			this->isPreview = false;
		}

		void Reset()
		{
			this->isPreview = false;
			this->isLine = false;
			this->isAlive = false;
			this->isFilled = false;
		}

		void DrawSelf(olc::PixelGameEngine* pge, bool debug = false)
		{
			if (debug)
			{
				if (isAlive)
					pge->DrawString(position, std::to_string(1), olc::DARK_BLUE);
				else if (isFilled)
					pge->DrawString(position, std::to_string(2), olc::RED);
				else if (isPreview)
					pge->DrawString(position, std::to_string(3), olc::DARK_MAGENTA);
				else if (isLine)
					pge->DrawString(position, std::to_string(4), olc::GREEN);
				else
					pge->DrawString(position, std::to_string(0), olc::BLUE);
			}
			else
			{
				if (isAlive || isFilled)
				{
					fillColor.a = 255;
					pge->FillRect(position, size, fillColor);
				}
				else if (isPreview)
				{
					fillColor.a = 120;
					pge->FillRect(position, size, fillColor);
				}
				else if (isLine)
					pge->FillRect(position, size, olc::DARK_GREEN);
			}

			pge->DrawRect(position, size, olc::BLACK);
		}
	};

	//field
	int width = 10;
	int height = 20;
	Cell* grid;
	olc::vi2d cellSize = olc::vi2d(15, 15);

	//game state
	GameState gameState = GameState::Running;
	int currentTetromino;
	int holdTetromino = -1;
	int nextTetromino;
	bool canHold = true;
	Rotation currentRotation = Rotation::Rotation_0;
	olc::vi2d currentPosition = olc::vi2d(3, 0);
	olc::vi2d previewPosition = olc::vi2d(3, 0);
	int score = 0;
	const int scoreByLine = 100;
	bool hasLines = false;
	bool debug = false;

	//timers
	float fallDownTimer = 0.0f;
	float fallDownTime = 1.2f; //time for the shape to fall by one in seconds
	const float fallDownTimeFloor = 0.2f; //limit of difficulty

	float downLockTimer = 0.0f;
	const float downLockTime = 0.075f; //Timer between forcing current shape down while down key is held

	float clearLinesTimer = 0.0f;
	const float clearLinesTime = 0.75f; //time between a line getting formed and it getting cleared

	float gameTime = 0.0f; //Time of the game

	const int difficutlyIncreaseTime = 20;//Time to change the current falldowntime by difficultyincreaseamount
	float difficutlyIncreaseAmount = -0.1f;
	bool difficultyIncreased = false; //flag to avoid multiple increases due to multiframe per second

	//Shapes
	TetroMino tetrominos[7]{
		TetroMino(olc::Pixel(80, 225, 253), //I
			new bool[16]{ 
				0, 0, 1, 0,
				0, 0, 1, 0,
				0, 0, 1, 0,
				0, 0, 1, 0 }),
		TetroMino(olc::Pixel(120,175,61), //Z
			new bool[16]{ 
				0, 0, 1, 0,
				0, 1, 1, 0,
				0, 1, 0, 0,
				0, 0, 0, 0 }),
		TetroMino(olc::Pixel(233, 60, 31), //S
			new bool[16]{
				0, 1, 0, 0,
				0, 1, 1, 0,
				0, 0, 1, 0,
				0, 0, 0, 0 }),
		TetroMino(olc::Pixel(254, 249, 77), //O
			new bool[16]{ 
				0, 1, 1, 0,
				0, 1, 1, 0,
				0, 0, 0, 0,
				0, 0, 0, 0 }),
		TetroMino(olc::Pixel(149,54,147), //T
			new bool[16]{ 
				0, 0, 1, 0,
				0, 1, 1, 0,
				0, 0, 1, 0,
				0, 0, 0, 0 }),
		TetroMino(olc::Pixel(246, 146, 48), //L
			new bool[16]{ 
				0, 1, 1, 0,
				0, 0, 1, 0,
				0, 0, 1, 0,
				0, 0, 0, 0 }),
		TetroMino(olc::Pixel(240, 110, 184), //J
			new bool[16]{ 
				0, 1, 1, 0,
				0, 1, 0, 0,
				0, 1, 0, 0,
				0, 0, 0, 0 }),
	};

	//Rotates a tetromino using x & y coords of its bool shape array
	int Rotate(int x, int y, Rotation r)
	{
		switch (r) {
		case Rotation::Rotation_0:
			return y * 4 + x;
		case Rotation::Rotation_90:
			return 12 + y - (x * 4);
		case Rotation::Rotation_180:
			return 15 - (y * 4) - x;
		case Rotation::Rotation_270:
			return 3 - y + (x * 4);
		}
	}

	//Is the current position of the tetromino valid
	bool IsValidPosition(int tetromino, Rotation rotation, olc::vi2d pos)
	{
		for (int x = 0; x < 4; x++)
			for (int y = 0; y < 4; y++)
			{
				int ri = Rotate(x, y, rotation); //Current rotation
				int gridIndex = (pos.y + y) * width + (pos.x + x); // Current position on the grid

				//Is the shape outside of the field bounds
				if (pos.x + x < 0 || pos.x + x > width - 1 || pos.y + y > height - 1)
					if (tetrominos[tetromino].shape[ri] == 1)
						return false;

				//Is the shape colliding with a block on the field
				if ((pos.x + x >= 0 && pos.x + x < width) && (pos.y + y >= 0 && pos.y + y < height))
					if (tetrominos[tetromino].shape[ri] == 1 && grid[gridIndex].isFilled)
						return false;
			}

		return true;
	}

	void DoClearLines()
	{
		int lastLine = 0, lineSum = 0;
		//clear lines
		for (int y = height - 1; y >= 0; y--)
		{
			int index = y * width;

			if (grid[index].isLine)
			{
				score += scoreByLine;
				lastLine = y;
				lineSum++;
				for (int x = 0; x < width; x++)
					grid[y * width + x].Reset();
			}
		}
		//move all peices from lastLine Y to 0 down by linesum
		for (int y = lastLine; y >= 0; y--)
		{
			for (int x = 0; x < width; x++)
			{
				int newIndex = (y + lineSum) * width + x;
				int oldIndex = y * width + x;

				if (grid[oldIndex].isFilled)
				{
					grid[newIndex].fillColor = grid[oldIndex].fillColor;
					grid[newIndex].isFilled = true;
					grid[oldIndex].Reset();
				}
			}
		}

		hasLines = false;
	}

	void DoPreview()
	{
		previewPosition.x = currentPosition.x;
		previewPosition.y = currentPosition.y;
		while (IsValidPosition(currentTetromino, currentRotation, olc::vi2d(previewPosition.x, previewPosition.y + 1)))
		{
			previewPosition.y++;
		}
	}

	void DoGameLogic()
	{
		//can shape go down?
		if (IsValidPosition(currentTetromino, currentRotation, olc::vi2d(currentPosition.x, currentPosition.y + 1)))
			currentPosition.y++; //go down
		else
		{
			//lock it to field
			for (int x = 0; x < 4; x++)
				for (int y = 0; y < 4; y++)
				{
					int j = (y + currentPosition.y);
					int i = (x + currentPosition.x);

					if (tetrominos[currentTetromino].shape[Rotate(x, y, currentRotation)] == true)
					{
						//set color
						grid[j * width + i].fillColor = tetrominos[currentTetromino].color;
						//is filled but no longer under player control
						grid[j * width + i].isAlive = false;
						grid[j * width + i].isFilled = true;
					}
				}
			//check if there is a line
			for (int i = 0; i < 4; i++)
			{
				int y = currentPosition.y + i;

				if (y < height)
				{
					bool isLine = true;

					for (int x = 0; x < width; x++)
						if (!grid[y * width + x].isFilled)
							isLine = false;
					if (isLine)
					{
						hasLines = true;

						for (int x = 0; x < width; x++)
						{
							grid[y * width + x].isFilled = false;
							grid[y * width + x].isLine = true;
						}
					}
				}
			}
			//get a new shape
			currentTetromino = nextTetromino;
			nextTetromino = rand() % 7;
			currentRotation = Rotation::Rotation_0;
			currentPosition = olc::vi2d(3, 0);
			canHold = true;
			//Check if validn if not game over!
			if (!IsValidPosition(currentTetromino, currentRotation, currentPosition))
				gameState = GameState::Over;
		}

		fallDownTimer = 0.0f;
	}

	void DoDrawGame()
	{
		//set current shape
		for (int x = 0; x < 4; x++)
			for (int y = 0; y < 4; y++)
			{
				int j = (y + currentPosition.y);
				int i = (x + currentPosition.x);

				if (tetrominos[currentTetromino].shape[Rotate(x, y, currentRotation)] == true)
				{
					//set color
					grid[j * width + i].fillColor = tetrominos[currentTetromino].color;
					//is currently under player control
					grid[j * width + i].isAlive = true;
				}
			}

		//set current preview
		for (int x = 0; x < 4; x++)
			for (int y = 0; y < 4; y++)
			{
				int j = (y + previewPosition.y);
				int i = (x + previewPosition.x);

				if (tetrominos[currentTetromino].shape[Rotate(x, y, currentRotation)] == true)
				{
					//set color
					grid[j * width + i].fillColor = tetrominos[currentTetromino].color;
					//is currently under player control
					grid[j * width + i].isPreview = true;
				}
			}

		//draw field
		for (int i = 0; i < width; i++)
			for (int j = 0; j < height; j++)
			{
				grid[j * width + i].DrawSelf(this, debug);

				//is no longer under player control until all checks are done so remove preview & alive cells
				grid[j * width + i].isAlive = false;
				grid[j * width + i].isPreview = false;
			}
	}

	void DoDrawInterface()
	{
		/*
		Of course everything is hardcoded
		Not good, yes
		Its a simple tetris so its ok, to me
		*/
		//get the draw pos to not overlap with grid !
		olc::vi2d drawPos((width * cellSize.x) + 15, 5);
		std::string drawText;

		//draw score
		drawText = "Score: " + std::to_string(score);
		DrawString(drawPos, drawText, olc::BLACK, 1U);
		drawPos.y += GetTextSize(drawText).y + 10;

		//draw time
		drawText = "Time: " + std::to_string((int)gameTime) + "s";
		DrawString(drawPos, drawText, olc::BLACK, 1U);
		drawPos.y += GetTextSize(drawText).y + 10;

		//draw hold
		drawText = "Hold: ";
		DrawString(drawPos, drawText, olc::BLACK, 1U);
		drawPos.y += GetTextSize(drawText).y + 5;
		for (int x = 0; x < 4; x++)
			for (int y = 0; y < 4; y++)
			{
				int index = Rotate(x, y, Rotation::Rotation_0);
				olc::vi2d pos = olc::vi2d(x * cellSize.x + drawPos.x, y * cellSize.y + drawPos.y);

				if (holdTetromino != -1 && tetrominos[holdTetromino].shape[index] == true)
					FillRect(pos, cellSize, tetrominos[holdTetromino].color);

				DrawRect(pos, cellSize, olc::BLACK);
			}
		drawPos.y += cellSize.y * 4 + 10;

		//draw next shape
		drawText = "Next: ";
		DrawString(drawPos, drawText, olc::BLACK, 1U);
		drawPos.y += GetTextSize(drawText).y + 5;
		for (int x = 0; x < 4; x++)
			for (int y = 0; y < 4; y++)
			{
				int index = y * 4 + x;
				olc::vi2d pos = olc::vi2d(x * cellSize.x + drawPos.x, y * cellSize.y + drawPos.y);

				if (tetrominos[nextTetromino].shape[index] == true)
					FillRect(pos, cellSize, tetrominos[nextTetromino].color);

				DrawRect(pos, cellSize, olc::BLACK);
			}

		drawPos.y += cellSize.y * 4 + 10;
		//Draw controls
		drawText = "Controls: \n- Move using left, right \nor down";
		DrawString(drawPos, drawText, olc::BLACK, 1U);
		drawPos.y += GetTextSize(drawText).y + 5;
		drawText = "- Froce fall using space";
		DrawString(drawPos, drawText, olc::BLACK, 1U);
		drawPos.y += GetTextSize(drawText).y + 5;
		drawText = "- Rotate using R";
		DrawString(drawPos, drawText, olc::BLACK, 1U);
		drawPos.y += GetTextSize(drawText).y + 5;
		drawText = "- Hold using H";
		DrawString(drawPos, drawText, olc::BLACK, 1U);
		drawPos.y += GetTextSize(drawText).y + 5;
		drawText = "- Pause using P";
		DrawString(drawPos, drawText, olc::BLACK, 1U);
		drawPos.y += GetTextSize(drawText).y + 5;
		drawText = "- Restart using Enter";
		DrawString(drawPos, drawText, olc::BLACK, 1U);
		drawPos.y += GetTextSize(drawText).y + 5;
	}

	void DoRestartGame()
	{
		//reset all data
		holdTetromino = -1;
		canHold = true;
		currentRotation = Rotation::Rotation_0;
		currentPosition = olc::vi2d(3, 0);
		previewPosition = olc::vi2d(3, 0);
		score = 0;
		hasLines = false;
		fallDownTimer = 0.0f;
		fallDownTime = 1.2f;
		downLockTimer = 0.0f;
		clearLinesTimer = 0.0f;
		gameTime = 0.0f;
		difficultyIncreased = false;
		srand((unsigned)time(NULL));
		currentTetromino = rand() % 7;
		srand((unsigned)time(NULL) + 1562);
		nextTetromino = rand() % 7;

		//set up grid
		grid = new Cell[width * height];
		for (int i = 0; i < width; i++)
			for (int j = 0; j < height; j++)
				grid[j * width + i] = Cell(olc::WHITE, false, olc::vi2d(i * cellSize.x, j * cellSize.y), cellSize);
	}

	void DoGamePaused()
	{
		std::string text = "Paused";
		olc::vi2d textSize = GetTextSize(text);

		int x = (ScreenWidth() / 2) - (textSize.x); //since scale is 2 then no need to devide and multiple by 2
		int y = (ScreenHeight() / 2) - (textSize.y);

		DrawString(olc::vi2d(x, y), text, olc::BLACK, 2U);
	}

	void DoGameOver()
	{
		std::string text = "Game over :(";
		olc::vi2d textSize = GetTextSize(text);

		int x = (ScreenWidth() / 2) - (textSize.x); //since scale is 2 then no need to devide and multiple by 2
		int y = (ScreenHeight() / 2) - (textSize.y);

		DrawString(olc::vi2d(x, y), text, olc::DARK_RED, 2U);
	}

	void DoPlayerInput(float fElapsedTime)
	{
		if ((GetKey(olc::LEFT).bPressed || GetKey(olc::Q).bPressed)
			&& IsValidPosition(currentTetromino, currentRotation, olc::vi2d(currentPosition.x - 1, currentPosition.y)))
			currentPosition.x--;
		if ((GetKey(olc::RIGHT).bPressed || GetKey(olc::D).bPressed)
			&& IsValidPosition(currentTetromino, currentRotation, olc::vi2d(currentPosition.x + 1, currentPosition.y)))
			currentPosition.x++;
		if ((GetKey(olc::DOWN).bHeld || GetKey(olc::S).bHeld)
			&& downLockTimer <= 0.0f)
		{
			//If downkey is pressed then call game logic
			downLockTimer = downLockTime;
			DoGameLogic();
		}
		else
			downLockTimer -= fElapsedTime;

		if (GetKey(olc::T).bPressed)
		{
			debug = !debug;
		}

		if (GetKey(olc::ENTER).bPressed)
		{
			DoRestartGame();
		}

		if (GetKey(olc::H).bPressed && canHold)
		{
			currentPosition.y = 0;
			currentPosition.x = 3;
			
			if (holdTetromino == -1)
			{
				holdTetromino = currentTetromino;
				currentTetromino = nextTetromino;
				nextTetromino = rand() % 7;
			}
			else
			{
				int old = currentTetromino;
				currentTetromino = holdTetromino;
				holdTetromino = old;
			}

			canHold = false;
		}

		if (GetKey(olc::R).bPressed)
		{
			switch (currentRotation)
			{
			case Tetris::Rotation::Rotation_0:
				if (IsValidPosition(currentTetromino, Rotation::Rotation_90, currentPosition))
					currentRotation = Rotation::Rotation_90;
				break;
			case Tetris::Rotation::Rotation_90:
				if (IsValidPosition(currentTetromino, Rotation::Rotation_180, currentPosition))
					currentRotation = Rotation::Rotation_180;
				break;
			case Tetris::Rotation::Rotation_180:
				if (IsValidPosition(currentTetromino, Rotation::Rotation_270, currentPosition))
					currentRotation = Rotation::Rotation_270;
				break;
			case Tetris::Rotation::Rotation_270:
				if (IsValidPosition(currentTetromino, Rotation::Rotation_0, currentPosition))
					currentRotation = Rotation::Rotation_0;
				break;
			}
		}
	}

public:
	Tetris()
	{
		sAppName = "TETRIS++";
	}

	bool OnUserCreate() override
	{
		SetPixelMode(olc::Pixel::ALPHA);
		srand((unsigned)time(NULL));
		currentTetromino = rand() % 7;
		srand((unsigned)time(NULL) + 1562);
		nextTetromino = rand() % 7;
		//set up grid
		grid = new Cell[width * height];
		for (int i = 0; i < width; i++)
			for (int j = 0; j < height; j++)
				grid[j * width + i] = Cell(olc::WHITE, false, olc::vi2d(i * cellSize.x, j * cellSize.y), cellSize);

		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override
	{
		Clear(olc::WHITE);

		if (GetKey(olc::P).bPressed)
		{
			gameState = gameState == GameState::Paused ? GameState::Running : GameState::Paused;
		}

		if (gameState == GameState::Running)
		{
			DoPlayerInput(fElapsedTime);

			//game logic
			if (fallDownTimer >= fallDownTime)
			{
				DoGameLogic();
			}
			else
				fallDownTimer += fElapsedTime;

			if (hasLines && clearLinesTimer >= clearLinesTime)
			{
				DoClearLines();
				clearLinesTimer = 0.0f;
			}
			else if (hasLines)
				clearLinesTimer += fElapsedTime;

			gameTime += fElapsedTime;

			//since i'm flooring, 0 % anything is 0, i don't want to add difficulty at the very start
			//not elegent, but works
			if (gameTime > 1.0f)
			{
				if (!difficultyIncreased && (int)std::floorf(gameTime) % difficutlyIncreaseTime == 0)
				{
					fallDownTime = fallDownTime + difficutlyIncreaseAmount < fallDownTimeFloor ?
						fallDownTimeFloor :
						fallDownTime + difficutlyIncreaseAmount;
					difficultyIncreased = true;
				}
				else if ((int)std::floorf(gameTime) % difficutlyIncreaseTime != 0)
					difficultyIncreased = false;
			}
		}

		DoPreview();
		DoDrawGame();
		DoDrawInterface();

		if (gameState == GameState::Paused)
		{
			DoGamePaused();
		}
		else if (gameState == GameState::Over)
		{
			DoGameOver();
		}

		return true;
	}
};

int main()
{
	Tetris app;
	if (app.Construct(366, 301, 2, 2))
		app.Start();
	return 0;
}