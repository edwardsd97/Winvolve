
#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <malloc.h>
#include <string.h>
#include <direct.h>
#include <time.h>
#include <math.h>

#include "evolve.h"

#pragma warning(disable:4996) // This function or variable may be unsafe.

#define max(a,b)		(((a) > (b)) ? (a) : (b))
#define min(a,b)		(((a) < (b)) ? (a) : (b))
#define clamp(a,b,c)    ( min( max(a, b), c ) )

creature_t creatures[MAX_POP];
creature_t environment[POP_COLS];
creature_t creatureLastAlive[2];
int creatureLastAliveIdx[2];

int orderTable[MAX_POP];
int population[2];

int speciesState[2];


char alphabet[] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 0 };
int  alphabet_size;

int rebirth = 0;
float predation = DEFAULT_PREDATION;
unsigned generation = 1;
unsigned species = 0;
unsigned extinctions = 0;
unsigned massExtinctions = 0;
int river_col = 0;


typedef float(*score_func)(creature_t *creature, creature_t *otherCreature);

char random_letter( int size = alphabet_size )
{
	return alphabet[rand() % size];
}

void randomize_order()
{
	for (int i = 0; i < MAX_POP; i++)
	{
		int swap = rand() % MAX_POP;
		int temp = orderTable[i];
		orderTable[i] = orderTable[swap];
		orderTable[swap] = temp;
	}
}

float randf()
{
	return float(rand() % 100) / 100.0f;
}

char alphabet_index( char c )
{
	int i;
	for (i = 0; i < alphabet_size; i++)
	{
		if (c == alphabet[i])
			return i;
	}

	return -1;
}

int creature_index(creature_t *creature)
{
	return creature - creatures;
}

int creature_row(creature_t *creature)
{
	return creature_index(creature) / POP_COLS;
}

int creature_col(creature_t *creature)
{
	return creature_index(creature) % POP_COLS;
}

creature_t *creature_environment(creature_t *creature)
{
	return &environment[creature_col(creature)];
}

void mutate(creature_t *creature)
{
	int gene = rand() % GENES_LEN;
	if (rand() % 2)
	{
		// Change case but keep letter
		if (creature->genes[gene] > 'Z')
			creature->genes[gene] = creature->genes[gene] - ('a' - 'A');
		else
			creature->genes[gene] = creature->genes[gene] + ('a' - 'A');
	}
	else
	{
		// Change letter but keep case
		int index = alphabet_index(creature->genes[gene]);
		index += rand() % 2 ? 1 : -1;
		if (creature->genes[gene] > 'Z')
			index = clamp(index, 0, (alphabet_size / 2) - 1);
		else
			index = clamp(index, alphabet_size / 2, alphabet_size - 1);
		creature->genes[gene] = alphabet[index];
	}
}

void breed_creature(creature_t *child, creature_t *parentA, creature_t *parentB)
{
	int i;
	if (parentA && !parentB)
	{
		// Clone of A
		strcpy(child->genes, parentA->genes);
	}
	else if (!parentA && parentB)
	{
		// Clone of B
		strcpy(child->genes, parentA->genes);
	}
	else if (parentA && parentB)
	{
		// Child of A and B
		for (i = 0; i < GENES_LEN; i++)
			child->genes[i] = rand() % 2 ? parentA->genes[i] : parentB->genes[i];
	}
	else
	{
		// Completely Random
		for (i = 0; i < GENES_LEN; i++)
			child->genes[i] = random_letter();
		child->genes[GENES_SIZE - 1] = 0;
	}

	// 1 Genetic Mutation
	mutate(child);

	child->age = 0;
}

void random_environments(creature_t *envs, int count)
{
	char rndA = random_letter(alphabet_size);
	char rndB = random_letter(alphabet_size);

	int i;
	int j;
	const int envMod = 4;

	// Bleed some creatures over to the other sides of the river
	if (river_col > 0 && river_col < POP_COLS - 1)
	{
		for (i = 0; i < POP_ROWS; i++)
		{
			if (rand() % 2)
			{
				// Swap
				creature_t temp;
				memcpy(&temp, &creatures[i*POP_COLS + river_col], sizeof(creature_t));
				memcpy(&creatures[i*POP_COLS + river_col], &creatures[i*POP_COLS + (river_col - 1)], sizeof(creature_t));
				memcpy(&creatures[i*POP_COLS + (river_col-1)], &temp, sizeof(creature_t));
			}
		}
	}

	river_col = POP_COLS / 2;// (int)((float(POP_COLS) / 3.0f) + (randf() * (float(POP_COLS) / 3.0f)));

	for (j = 0; j < count; j++)
	{
		if (j == river_col)
		{
			rndA = random_letter(alphabet_size);
			rndB = random_letter(alphabet_size);
		}
		for (i = 0; i < GENES_SIZE - 1; i++)
		{
			if (i < GENES_LEN / 2)
				envs[j].genes[i] = rndA;
			else
				envs[j].genes[i] = rndB;
		}

		envs[j].genes[GENES_SIZE - 1] = 0;
	}

	//strcpy(environment.genes, "aaaaaaaa");
}

int letter_delta(char A, char B)
{
	if (A < 'a')
		A += 'a' - 'A';
	if (B < 'a')
		B += 'a' - 'A';

	if (A == B)
		return 0;

	int bigger = max(A, B);
	int smaller = min(A, B);

	int result1 = bigger - smaller;
	int result2 = 1 + (smaller - alphabet[0]) + (alphabet[(alphabet_size / 2)-1]) - bigger;

	return min( result1, result2 ) * 2;
}

float matches_letter(char A, char B)
{
	float diff = fabs((float)letter_delta(A, B));
	float range = (float(alphabet_size) / 2.0f);

	return clamp(1.0f - (diff / range), 0.0f, 1.0f);
}

float matches_case(char A, char B)
{
	if (B >= 'a' && A >= 'a')
		return 1.0f;
	if (B < 'a' && A < 'a')
		return 1.0f;
	return 0.0f;
}

float score_connected(creature_t *creatureA, creature_t *creatureB)
{
	int rowA = creature_row(creatureA);
	int colA = creature_col(creatureA);

	int rowB = creature_row(creatureB);
	int colB = creature_col(creatureB);

	if ((colA < river_col && colB >= river_col) ||
		(colB < river_col && colA >= river_col))
		return -1.0f; // across the river

	return 1.0f;
}

float score_predatory(creature_t *triggerer, creature_t *prey)
{
	if (!triggerer->genes[0])
		return -1.0f; // triggerer is dead

	if (!prey->genes[0])
		return -1.0f; // prey is dead

	if (creature_col(triggerer) != creature_col(prey))
		return -1.0f;

	float score = 0;
	for (int i = 0; i < GENES_LEN; i++)
	{
		// Prefer those that stand out against the environment
		score += (1.0f - matches_case(prey->genes[i], creature_environment(prey)->genes[i])) * (1.0f / (float)(GENES_LEN * 8));
		score += (1.0f - matches_letter(prey->genes[i], creature_environment(prey)->genes[i])) * (1.0f / (float)(GENES_LEN));
	}

	return score;
}

float score_mate(creature_t *seeker, creature_t *victim)
{
	if (!seeker->genes[0])
		return -1; // i am dead

	if (!victim->genes[0])
		return -1; // victim dead

	if (victim->age < AGE_MATURE)
		return -1; // victim not old enough

	if (seeker->age < AGE_MATURE)
		return -1; // i am not old enough

	if (seeker == victim)
		return -1; // cant mate with myself

	if (score_connected(seeker, victim) <= 0.0f)
		return -1;

	float score = 0;

	for (int i = 0; i < GENES_LEN; i++)
	{
		// Prefer those that match my colors the most
		score += matches_letter(seeker->genes[i], victim->genes[i]) * (1.0f / (float)(GENES_LEN*2));

		// Prefer those that stand out against the environment
		score += (1.0f - matches_letter(victim->genes[i], creature_environment(victim)->genes[i])) * (1.0f / (float)(GENES_LEN * 2));
	}

	return score;
}

float score_near(creature_t *creatureA, creature_t *creatureB)
{
	if (score_connected(creatureA, creatureB) <= 0.0f)
		return -1.0f;

	int rowDelta = abs(creature_row(creatureB) - creature_row(creatureA));
	int colDelta = abs(creature_row(creatureB) - creature_row(creatureA));
	float diff = (float)max(rowDelta, colDelta);
	float range = max( POP_ROWS, POP_COLS );
	return clamp(1.0f - (diff / range), 0.0f, 1.0f);
}

float score_free(creature_t *creatureA, creature_t *creatureB)
{
	if (creatureB->genes[0])
		return -1.0f; // not free

	if (creatureA == creatureB)
		return -1.0f; // same creature

	// free slot
	return score_near(creatureA, creatureB);
}

int find_best(creature_t *creature, score_func func)
{
	float bestScore = 0;
	int i;
	int result = -1;

	for (i = 0; i < MAX_POP; i++)
	{
		float score = func(creature, &creatures[i]);
		if ((score > bestScore) || (score == bestScore && rand() % 2))
		{
			bestScore = score;
			result = i;
		}
	}

	return result;
}

void die( creature_t *creature )
{
	if (creature->genes[0])
	{
		if (creature_col(creature) >= river_col)
		{
			strcpy(creatureLastAlive[1].genes, creature->genes);
			creatureLastAliveIdx[1] = creature_index(creature);
		}
		else
		{
			strcpy(creatureLastAlive[0].genes, creature->genes);
			creatureLastAliveIdx[0] = creature_index(creature);
		}
		memset(creature, 0, sizeof(creature_t));
	}
}

void survive( int iCreature )
{
	if (!creatures[iCreature].genes[0])
		return; // dead creature

	// Age
	(creatures[iCreature].age)++;
	if (creatures[iCreature].age >= AGE_DEATH)
	{
		die(&creatures[iCreature]);
		return;
	}

	if (predation > 0.0f && rebirth <= 0)
	{
		// Predation: See if one of us getss eaten
		int eaten = find_best(&creatures[iCreature], score_predatory);
		if (eaten >= 0)
		{
			float score = score_predatory(&creatures[eaten], &creatures[eaten]);
			if ( randf() < score * predation )
				die(&creatures[eaten]);
		}
	}
	else if (rebirth < REBIRTH_GENERATIONS - 3 && creatures[iCreature].age >= (AGE_DEATH * 0.5f))
	{
		// Rebirth: Die of old age with a bit of randomness toward the end
		if ( randf() < (float)(creatures[iCreature].age) / ((float)AGE_DEATH))
			die(&creatures[iCreature]);
	}
}

void procreate( int iCreature )
{
	// Find best creature to mate
	int mate = find_best(&creatures[iCreature], score_mate);
	int free = find_best(&creatures[iCreature], score_free);
	if (mate >= 0 && free >= 0)
	{
		//float score = score_predatory(&creatures[iCreature], &creatures[mate]);
		//if (randf() < score * 4.0f)
		{
			breed_creature(&creatures[free], &creatures[iCreature], &creatures[mate]);
		}
	}
}

void current_population( )
{
	population[0] = 0;
	population[1] = 0;
	for (int i = 0; i < MAX_POP; i++)
	{
		if (creature_col(&creatures[i]) < river_col)
			population[0] += (creatures[i].genes[0] != 0);
		else
			population[1] += (creatures[i].genes[0] != 0);
	}
}

void evolve_rebirth()
{
	int rebirth_age = max(0, AGE_MATURE - 1);

	for (int i = 0; i < MAX_POP; i++)
		creatures[i].age = rebirth_age;

	random_environments(environment, POP_COLS);

	for (int col = river_col; col >= 0 && col > river_col - 2; col--)
	{
		if (col >= river_col)
		{
			if (population[1] < 2)
			{
				int spawnRow = rand() % POP_ROWS;
				for (int i = 0; i < (2 - population[1]); i++)
				{
					int spawnIdx = (spawnRow*POP_COLS) + river_col;
					mutate(&creatureLastAlive[1]);
					int free = find_best(&creatures[spawnIdx], score_free);
					if (free >= 0)
					{
						breed_creature(&creatures[free], &creatureLastAlive[1], NULL);
						creatures[free].age = rebirth_age;
					}
				}
			}
		}
		else if (river_col > 0)
		{
			if (population[0] < 2)
			{
				int spawnRow = rand() % POP_ROWS;
				for (int i = 0; i < (2 - population[0]); i++)
				{
					int spawnIdx = (spawnRow*POP_COLS) + (river_col - 1);
					mutate(&creatureLastAlive[0]);
					int free = find_best(&creatures[spawnIdx], score_free);
					if (free >= 0)
					{
						breed_creature(&creatures[free], &creatureLastAlive[0], NULL);
						creatures[free].age = rebirth_age;
					}
				}
			}
		}
	}

	rebirth = REBIRTH_GENERATIONS;
}

void evolve_init()
{
	alphabet_size = strlen(alphabet);

	srand((unsigned)time(NULL));

	random_environments(environment, POP_COLS);

	// Generate a single random creature in each environment and trigger a time of rebirth
	memset(creatures, 0, sizeof(creatures));
	for (int i = 0; i < MAX_POP; i++)
		orderTable[i] = i;

	for (int col = river_col; col >= 0 && col > river_col - 2; col--)
	{
		int spawnRow = rand() % POP_ROWS;
		int spawnIdx = (spawnRow*POP_COLS) + col;
		breed_creature(&creatures[spawnIdx], NULL, NULL);
		int spouseIdx = find_best(&creatures[spawnIdx], score_free);
		if ( spouseIdx >= 0 )
			breed_creature(&creatures[spouseIdx], &creatures[spawnIdx], NULL);
		if (col >= river_col)
		{
			strcpy(creatureLastAlive[1].genes, creatures[spawnIdx].genes);
			creatureLastAliveIdx[1] = spawnIdx;
		}
		else
		{
			strcpy(creatureLastAlive[0].genes, creatures[spawnIdx].genes);
			creatureLastAliveIdx[0] = spawnIdx;
		}
	}

	// Start with distinct species
	species++;
	speciesState[0] = 1;
	species++;
	speciesState[1] = 1;

	evolve_rebirth();
	randomize_order();
}

int evolve_simulate(int step)
{
	current_population();

	for (int i = 0; i < 2; i++)
	{
		if (!speciesState[i] && population[i] >= (MAX_POP / 2) / 2)
		{
			species++;
			speciesState[i] = 1;
		}
		else if ( speciesState[i] && population[i] <= 0 )
		{
			extinctions++;
			speciesState[i] = 0;
		}
	}

	if ((rebirth <= 0 && population[0] < 2 && population[1] < 2 )|| (population[0] == 0 && population[1] == 0))
	{
		massExtinctions++;

		// Initiate a rebirth
		evolve_rebirth();
		step = 0;
	}

	if (step >= (MAX_POP * 2))
	{
		generation++;
		if (generation % 100 == 0)
		{
			random_environments(environment, POP_COLS);
			randomize_order();
		}

		rebirth--;
		return 0;
	}
	else if (step % 2 == 0)
		survive(orderTable[step/2]);
	else if ( step %2 == 1)
		procreate(orderTable[step / 2]);

	if (population[0] == 0 && population[1] == 0)
		return 0;
	
	return step + 1;
}

void evolve_asteroid()
{
	randomize_order();
	for (int i = 0; i < MAX_POP; i++)
		die(&creatures[orderTable[i]]);
	rebirth = 0;
}

void evolve_earthquake()
{
	randomize_order();
	for (int i = 0; i < MAX_POP; i++)
	{
		if (rand() % 4 == 0)
			die(&creatures[orderTable[i]]);
	}

	random_environments(environment, POP_COLS);
}
