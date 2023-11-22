#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "lib/cbmp.h"

typedef unsigned int uint;
typedef unsigned char uchar;
typedef char bool;
/*
normal
{{14, 29, 30, 31, 16},
{16, 17, 2, -13, -14}};

{{0, 1}, {1, 0}, {0, -1}, {-1, 0}};

diag
{{30, 31, 32, 17, 2},
{-30, -29, -28, -13, 2}};

{{1, 1}, {1, -1}, {-1, -1}, {-1, 1}};
*/
const char checkPatterns[2][5] = {{14, 29, 30, 31, 16},
																	{16, 17, 2, -13, -14}};
const char dirNormals[4][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};

const double axisBias = 0.5; // 0 = horizontal only, 1 = vertical only
const double straightBias = 0.5;
const double momentumBias = 0.5;

BMP* bmp;

void indexToPos(char index, char* x, char* y){
	*x = (((abs(index) + 7) % 15) - 7) * ((index > 0) - (index < 0));
	*y = (index - x[0]) / 15;
}

uchar dirRotated(uchar direction, char rotation){
	while (rotation < 0){
		rotation += 4;
	}
	return (direction + rotation) % 4;
}

bool checkMove(uchar dir, uint x, uint y){
	uchar hits = 0;
	uchar axis = dir % 2;
	char sign = ((dir < 2) - (dir > 1));
	for (uchar posIndex = 0; posIndex < sizeof(checkPatterns[0]) / sizeof(checkPatterns[0][0]); posIndex++) {
		char offsetx, offsety;
		indexToPos(checkPatterns[axis][posIndex] * sign, &offsetx, &offsety);
		hits += (get_pixel_r(bmp, x + offsetx, y + offsety) > 0) ? 1 : 0;
	}
	hits += (get_pixel_r(bmp, x + dirNormals[dir][0], y + dirNormals[dir][1]) > 0) ? 1 : 0;
	return (hits > 0) ? 0 : 1;
}

uchar findBacktrack(uchar currentDistance, uint x, uint y){
	uchar r, g, b;
	currentDistance--; if (currentDistance == 0) currentDistance--;
	for (uchar dir = 0; dir < 4; dir++) {
		get_pixel_rgb(bmp, x + dirNormals[dir][0], y + dirNormals[dir][1], &r, &g, &b);
		if (g == currentDistance && r != 0){
			return dir;
		}
	}
	fprintf(stderr, "stranded\n");
	return 10;
}

void setup(int argc, char** argv){
	// Set random seed
	if (argc == 6){
		srand(atoi(argv[5]));
	} else {
		srand(time(NULL));
	}

	// Verify inputs
  if (argc != 5 && argc != 6){
    fprintf(stderr, "Usage: %s <input file> <output file> <[uint] startx> <[uint] starty> <(optional) [int] seed>\n", argv[0]);
    exit(1);
  }

  // Read image into BMP struct
  bmp = bopen(argv[1]);
}

int main(int argc, char** argv){
	// Run setup
	setup(argc, argv);

	// Create variables
	uint startx, starty;
  uint x, y;
	uchar distance = 1;
	uchar moveDir = 5; // 5 So it can be checked by straight bias
	uchar lastTurnDir = 5;

	// Assign start values
	startx = x = atoi(argv[3]);
	starty = y = atoi(argv[4]);

	// Set Start pixel
	set_pixel_rgb(bmp, x, y, 5, 0, 255);

	// Main loop
	printf("Starting loop...\n\n");
	unsigned long stepCounter = 0;
  while ((x != startx || y != starty || stepCounter == 0)){
		stepCounter++;

		// Find possible move directions
		bool backtrack = 0;

		uchar possDirs[4];
		double possChance[4];
		uchar possCount = 0;
		double possTotal = 0;
		for (uchar dir = 0; dir < 4; dir++) {
			if (checkMove(dir, x, y) == 1){
				possDirs[possCount] = dir;
				possChance[possCount] = ((dir == moveDir) ? straightBias : (1 - straightBias)) *
																((dir % 2 == 0) ? axisBias : (1 - axisBias)) *
																((dir != lastTurnDir) ? momentumBias : (1 - momentumBias));

				possTotal += possChance[possCount];
				possCount++;
			}
		}

		// Decide on move Dir
		if (possCount != 0){
			double random = ((double)rand() / RAND_MAX) * possTotal;
			double possMatch = 0; // Running total (needed)
			for (uchar poss = 0; poss < possCount; poss++) {
				possMatch += possChance[poss];
				if (random <= possMatch){
					if (possDirs[poss] != moveDir) lastTurnDir = moveDir; // Determine turn for momentumBias

					moveDir = possDirs[poss];
					goto chosen;
				}
			}
			fprintf(stderr, "Failed to choose\n");
			goto finish;
			chosen:
			distance++; if (distance == 0) distance++;
		} else {
			// Backtrack
			if (findBacktrack(distance, x, y) == 10) goto finish;
			moveDir = findBacktrack(distance, x, y);
			backtrack = 1;
			distance--; if (distance == 0) distance--;

			lastTurnDir = 5; // Determine turn for momentumBias
		}

		// Move
		x += dirNormals[moveDir][0];
		y += dirNormals[moveDir][1];

		// Set pixel value
		if (!backtrack){
			set_pixel_rgb(bmp, x, y, 255, distance, 0);
		}
  }

finish:

		// Show finish point
		printf("finish pos: %d %d\n", x, y);
		printf("steps: %lu\n", stepCounter);
		set_pixel_rgb(bmp, x, y, 5, 255, 0);

	  // Write bmp contents to file
	  bwrite(bmp, argv[2]);

	  // Free memory
	  bclose(bmp);

		printf("saved!\n");
	  return 0;
}
