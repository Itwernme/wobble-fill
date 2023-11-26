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

spaced (14)
{{13, 14, 16, 17, 28, 29, 30, 31, 32, 43, 44, 45, 46, 47},
{31, 32, 33, 16, 17, 18, 2, 3, -14, -13, -12, -29, -28, -27}};
(more efficeint approximation)
{{13, 28, 43, 44, 45, 46, 47, 32, 17},
{31, 32, 33, 18, 3, -12, -27, -28, -29}};

super spaced (28)
{{57, 58, 59, 60, 61, 62, 63, 42, 43, 44, 45, 46, 47, 48, 27, 28, 29, 30, 31, 32, 33, 12, 13, 14, 15, 16, 17, 18},
{46, 47, 48, 49, 31, 32, 33, 34, 16, 17, 18, 19, 1, 2, 3, 4, -14, -13, -12, -11, -29, -28, -27, -26, -44, -43, -42, -41}};

super mega spaced insecure (23)
{{14, 13, 12, 11, 26, 41, 56, 71, 72, 73, 74, 75, 76, 77, 78, 79, 64, 49, 34, 19, 18, 17, 16},
{16, 31, 46, 61, 62, 63, 64, 65, 50, 35, 20, 5, -10, -25, -40, -55, -56, -57, -58, 59, -44, -29, -14}};
*/
const char checkPatterns[2][5] =
{{14, 29, 30, 31, 16},
{16, 17, 2, -13, -14}};
const char dirNormals[4][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};

const double axisBias = 0.5; // 0 = horizontal, 1 = vertical
const double straightBias = 0.1;
const double momentumBias = 0.5;

BMP* bmp;
uchar pass = 1;

void indexToPos(char index, char* x, char* y){
	*x = (((abs(index) + 7) % 15) - 7) * ((index > 0) - (index < 0));
	*y = (index - x[0]) / 15;
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

uchar findPath(uint x, uint y, uchar lastDir, uchar lastTurnDir, uchar* currentDistance){
	// Find possible move directions
	uchar possDirs[4];
	double possChance[4];
	uchar possCount = 0;
	double possTotal = 0;

	for (uchar dir = 0; dir < 4; dir++) {
		if (checkMove(dir, x, y) == 1){
			possDirs[possCount] = dir;
			possChance[possCount] = ((dir == lastDir) ? straightBias : (1 - straightBias)) *
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
				if (possDirs[poss] != lastDir) lastTurnDir = lastDir; // Determine turn for momentumBias

				(*currentDistance)++; if ((*currentDistance) == 0) (*currentDistance)++;
				return possDirs[poss];
			}
		}
	}
	return 4;
}

void getSurroundings(uint x, uint y, pixel* out){
	for (uchar dir = 0; dir < 4; dir++) {
		pixel pix;
		get_pixel(bmp, x + dirNormals[dir][0], y + dirNormals[dir][1], &pix);
		out[dir] = pix;
	}
}

uchar findForwardtrack(pixel* surround){
	for (uchar dir = 0; dir < 4; dir++) {
		if (pass % 2 == 0){
			if ((&surround[dir])->blue != 0 && (&surround[dir])->red != 0) return dir;
		} else {
			if ((&surround[dir])->green != 0 && (&surround[dir])->red != 0) return dir;
		}
	}
	return 4;
}

uchar findBacktrack(uchar currentDistance, pixel* surround){
	currentDistance--; if (currentDistance == 0) currentDistance--;
	for (uchar dir = 0; dir < 4; dir++) {
		if (pass % 2 == 0){
			if ((&surround[dir])->green == currentDistance && (&surround[dir])->red != 0) return dir;
		} else {
			if ((&surround[dir])->blue == currentDistance && (&surround[dir])->red != 0) return dir;
		}
	}
	printf("stranded\n");
	return 4;
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
  uint x, y;
	uchar distance = 1;
	uchar moveDir = 4; // 4 So it can be checked by straight bias
	uchar lastTurnDir = 4;
	bool backtrack = 0;

	// Assign start values
	x = atoi(argv[3]);
	y = atoi(argv[4]);

	// Set Start pixel
	if (pass % 2 == 0) set_pixel_rgb(bmp, x, y, 255, distance, 0);
	else set_pixel_rgb(bmp, x, y, 255, 0, distance);

	// Main loop
	printf("Starting loop...\n\n");
	unsigned long stepCounter = 0;
  while (1){
		stepCounter++;

		moveDir = findPath(x, y, moveDir, lastTurnDir, &distance);
		backtrack = 0;

		if (moveDir == 4){
			pixel surroundings[4];
			getSurroundings(x, y, surroundings);
			moveDir = findForwardtrack(surroundings);

			if (moveDir == 4){
				moveDir = findBacktrack(distance, surroundings);

				if (moveDir == 4) goto finish;

				backtrack = 1;
				distance--; if (distance == 0) distance--;
			} else {
				distance++; if (distance == 0) distance++;
			}

			lastTurnDir = 4; // Determine turn for momentumBias
		}

		// Move
		x += dirNormals[moveDir][0];
		y += dirNormals[moveDir][1];

		// Set pixel value
		if (backtrack == 0){
			if (pass % 2 == 0) set_pixel_rgb(bmp, x, y, 255, distance, 0);
			else set_pixel_rgb(bmp, x, y, 255, 0, distance);
		}
  }

finish:

		// Show finish point
		printf("finish pos: %d %d\n", x, y);
		printf("steps: %lu\n", stepCounter);
		//set_pixel_rgb(bmp, x, y, 5, 255, 100);

	  // Write bmp contents to file
	  bwrite(bmp, argv[2]);

	  // Free memory
	  bclose(bmp);

		printf("saved!\n");
	  return 0;
}
