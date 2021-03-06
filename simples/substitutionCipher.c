#include <functions.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#define DICTIONARY " portuguese_dictionary.txt\0"
#define TABLE(i) (tableOfSubstitutionChars[alphabet[(i)]])
#define CHAR(c) (tableAlphabetIndex[(c)])
#define FORWARD 1
#define BACKWARD 0
#define LENGTH_COMP strlen("computador ")
#define LENGTH_SEG strlen("seguranca ")

extern const char alphabet[];

/**
 *	Stores forwards substitution
 */
static char tableOfSubstitutionChars[LAST_ASCII_CHAR_INDEX] = {0};

/** 
 *	Stores backwards substitution
 */	
static char tableOfSubstitutionCharsInv[LAST_ASCII_CHAR_INDEX] = {0};

/**
 *	Stores customized ASCII indexes
 */
static char tableAlphabetIndex[LAST_ASCII_CHAR_INDEX] = {0};

/**
 *	Stores customized ASCII indexes
 */
static char tableAlphabetCracked[LAST_ASCII_CHAR_INDEX] = {0};

/** 
 *	Stores characteres that need to shift (because of key bit)
 */
static char charsNeededToMove[ALPHABET_LENGTH] = {0};	

static void printTables()
{
	// DEBUGing
	if(DEBUG)
	{
		int i;
		printf("\n*** DEBUG: Substitution table:");
		printf("\n*** DEBUG: Alphabet:");
		for(i = 0; i < ALPHABET_LENGTH; i++)
			printf("%c ", alphabet[i]);

		printf("\n*** DEBUG: SubTable:");
		for(i = 0; i < ALPHABET_LENGTH; i++)
			printf("%c ", TABLE(i));

		printf("\n*** DEBUG: InvTable:");
		for(i = 0; i < ALPHABET_LENGTH; i++)
			printf("%c ", tableOfSubstitutionCharsInv[alphabet[i]]);
		printf("\n");
	}
}

// 1 - Little endian
// 0 - Big endian
static char isLittleEndian()
{
	static char endianess = -1;
	if(endianess != -1)
		return endianess;
	int number = 1;
	endianess = (*((char *)(&number)) & 1);
	return endianess;
}

static char * shell_exec(char * cmd)
{
	FILE * pipe = popen(cmd, "r");
	if(pipe == NULL)
		return NULL;

	char * buffer = (char *)malloc(sizeof(char) * 128);

	if(fgets(buffer, 128, pipe) != NULL)
		return buffer;
	//free(buffer);
	return NULL;
}

static void strtolower(char * str, size_t length)
{
	int i;
	for(i = 0; i < length; i++)
		str[i] = tolower(str[i]);
}

static unsigned char getByte(int key, int ith)
{
	return (isLittleEndian() ?
		* (((char *)(&key)) + (ith % sizeof(int))):
		* (((char *)(&key)) + ((DOUBLE_KEY_SIZE - ith) % sizeof(int)))
	);
}

static void passWhereKeyBitIs1(int key)
{
	unsigned char byte;
	int byteIndex, bitIndex, alphIndex, zeroIndex = 0;
	char leftOrRight = 0; // 0 - L, 1 - R
	char substituteChar;
	// Now, pass all posititions where key bit == 1
	for(byteIndex = 0; byteIndex < DOUBLE_KEY_SIZE; byteIndex++)
	{
		alphIndex = byteIndex * 8;
		byte = getByte(key, byteIndex);
		for(bitIndex = 0; bitIndex < 8 && alphIndex < ALPHABET_LENGTH; bitIndex++, alphIndex++)
		{
			if(byte & (1 << bitIndex))
			{
				TABLE(alphIndex) = alphabet[alphIndex];
				//printf("TABLE(alphIndex) = %c\n", alphabet[alphIndex]);
			}
			else
			{
				//printf("TABLE(alphIndex) = -\n");
				TABLE(alphIndex) = '-';
				charsNeededToMove[zeroIndex++] = alphIndex + 1;
			}
		}
	}
}

static void shiftPosition(char alphIndex, char * lastLeft, char * lastRight, char * leftOrRight)
{
	char oldChar;
	char subsIndex;
	char ll, lr, lor;

	ll  = *lastLeft;
	lr  = *lastRight;
	lor = *leftOrRight;
	oldChar = alphabet[alphIndex];
	//printf("Finding a new position for %c\n", oldChar);

	// Right
	if(lor)
	{
		while(TABLE(--lr) != '-' && lr >= 0)
			;//printf("R: lr = %i, TABLE(lr) = %c\n", lr, TABLE(lr));
		subsIndex = lr;
	}
	// Left
	else
	{
		while(TABLE(++ll) != '-' && ll < ALPHABET_LENGTH)
			;//printf("L: ll = %i, TABLE(ll) = %c\n", ll, TABLE(ll));
		subsIndex = ll;
	}
	//printf("New position is %c\n", alphabet[subsIndex]);

	lor = oldChar & 1;

	TABLE(subsIndex) = oldChar;

	*lastLeft 	= ll;
	*lastRight 	= lr;
	*leftOrRight= lor;
}

static void crazilyShift()
{	
	char alphIndex, lastLeft, lastRight, leftOrRight;

	// Helps to find next left or right
	lastRight = ALPHABET_LENGTH;
	lastLeft = -1;
	leftOrRight = 0;

	// Let the duty begin
	for(alphIndex = strlen(charsNeededToMove) - 1; alphIndex >= 0; alphIndex--)
		shiftPosition(charsNeededToMove[alphIndex] - 1, &lastLeft, &lastRight, &leftOrRight);
}

static char initTableOfSubstitutionChars(int key)
{
	// Copy the same characters where the key bit == 1
	passWhereKeyBitIs1(key);

	// Make a kinda crazy shift
	crazilyShift();

	// Create the inverted and alphabet index tables
	int i;
	for(i = 0; i < ALPHABET_LENGTH; i++)
	{
		//printf("For TABLE(i) %c=>%c \n", alphabet[i], TABLE(i));
		tableOfSubstitutionCharsInv[TABLE(i)] = alphabet[i];

		// Finally build the alphabet index, to ease calculations
		tableAlphabetIndex[alphabet[i]] = i;
	}

	printTables();
	return 1;
}

static char * substitute(const char * plainText, size_t length, int key, char forwardOrBackward)
{
	// Make sure substitution table is ok
	static int oldKey = 0;
	if(oldKey != key)
	{
		initTableOfSubstitutionChars(key);
		oldKey = key;
	}

	int cipherIndex = 0;
	char * cipheredText = (char *)malloc(sizeof(char) * length);
	char oldChar;

	for(; cipherIndex < length; cipherIndex++)
	{
		oldChar = plainText[cipherIndex];
		cipheredText[cipherIndex] = (forwardOrBackward ? 
										tableOfSubstitutionChars[oldChar] :
										tableOfSubstitutionCharsInv[oldChar]);
	}
	return cipheredText;
}

char * substitutionCipher(const char * plainText, size_t length, int key)
{
	if(!plainText)
		return;

	char * cipheredText = substitute(plainText, length, key, FORWARD);

	return cipheredText;
}

char * substitutionDecipher(const char * cipheredText, size_t length, int key)
{
	if(!cipheredText)
		return NULL;

	char * plainText = substitute(cipheredText, length, key, BACKWARD);

	return plainText;
}

char * getSimilarComputadorWords(const char * cipheredText, size_t length)
{
	if(!cipheredText)
		return NULL;

	char * similarComputadorWords = (char *)malloc(sizeof(char) * length * LENGTH_COMP);
	int indexSimilarComputadorWords = 0;

	// For computador
	int Ci, lastWord = length - LENGTH_COMP;
	char firstChar, lastChar, charC, charO1, charM, charP, charU, charT, charA, charD, charO2, charR;
	for(Ci = 0; Ci < lastWord; Ci++)
	{
		charC = cipheredText[Ci];
		if((charO1 = cipheredText[Ci + 1]) != charC)
		if((charM  = cipheredText[Ci + 2]) != charO1 
			&& charM != charC)
		if((charP  = cipheredText[Ci + 3]) != charM 
			&& charP != charC && charP != charO1)
		if((charU  = cipheredText[Ci + 4]) != charP
			&& charU != charC && charU != charO1 && charU != charM)
		if((charT  = cipheredText[Ci + 5]) != charU
			&& charT != charC && charT != charO1 && charT != charM 
			&& charT != charP)
		if((charA  = cipheredText[Ci + 6]) != charT
			&& charA != charC && charA != charO1 && charA != charM 
			&& charA != charP && charA != charU)
		if((charD  = cipheredText[Ci + 7]) != charA
			&& charD != charC && charD != charO1 && charD != charM 
			&& charD != charP && charD != charU  && charD != charT)
		if((charO2 = cipheredText[Ci + 8]) == charO1)
		if((charR  = cipheredText[Ci + 9]) != charO2 
			&& charR != charC && charR != charM && charR != charP
			&& charR != charU && charR != charT	&& charR != charA
			&& charR != charD)
		if(
			// First word
			(Ci == 0 && (lastChar = cipheredText[Ci + 10]) != charR
				&& lastChar != charC && lastChar != charO1 && lastChar != charM
				&& lastChar != charP && lastChar != charU  && lastChar != charT
				&& lastChar != charA && lastChar != charD
			)
			||
			// Last word
			(Ci == lastWord - 1 && (firstChar = cipheredText[Ci - 1]) != charR
				&& firstChar != charC && firstChar != charO1 && firstChar != charM
				&& firstChar != charP && firstChar != charU  && firstChar != charT
				&& firstChar != charA && firstChar != charD
			)
			||
			// Between spaces
			((firstChar = cipheredText[Ci - 1]) == (lastChar = cipheredText[Ci + 10])
				&& firstChar != charC && firstChar != charO1 && firstChar != charM
				&& firstChar != charP && firstChar != charU  && firstChar != charT
				&& firstChar != charA && firstChar != charD && firstChar != charR
			)
		)
		{
			// Finally it matches
			similarComputadorWords[indexSimilarComputadorWords++] = charC;
			similarComputadorWords[indexSimilarComputadorWords++] = charO1;
			similarComputadorWords[indexSimilarComputadorWords++] = charM;
			similarComputadorWords[indexSimilarComputadorWords++] = charP;
			similarComputadorWords[indexSimilarComputadorWords++] = charU;
			similarComputadorWords[indexSimilarComputadorWords++] = charT;
			similarComputadorWords[indexSimilarComputadorWords++] = charA;
			similarComputadorWords[indexSimilarComputadorWords++] = charD;
			similarComputadorWords[indexSimilarComputadorWords++] = charO2;
			similarComputadorWords[indexSimilarComputadorWords++] = charR;
			similarComputadorWords[indexSimilarComputadorWords++] = (Ci ? firstChar : lastChar);
			similarComputadorWords[indexSimilarComputadorWords++] = ',';
		}
	}
	if(indexSimilarComputadorWords)
		return similarComputadorWords;
	free(similarComputadorWords);
	return NULL;
}

char * getSimilarSegurancaWords(const char * cipheredText, size_t length)
{
	if(!cipheredText)
		return NULL;
	char * similarSegurancaWords = (char *)malloc(sizeof(char) * length * LENGTH_SEG);
	int indexSimilarSegurancaWords = 0;

	// For Seguranca
	int Ci, lastWord = length - LENGTH_SEG;
	char firstChar, lastChar, charS, charE, charG, charU, charR, charA1, charN, charC, charA2;
	for(Ci = 0; Ci < lastWord; Ci++)
	{
		charS = cipheredText[Ci];
		if((charE = cipheredText[Ci + 1]) != charS)
		if((charG = cipheredText[Ci + 2]) != charE 
			&& charG != charS)
		if((charU = cipheredText[Ci + 3]) != charG 
			&& charU != charS && charU != charE)
		if((charR  = cipheredText[Ci + 4]) != charU
			&& charR != charS && charR != charE && charR != charG)
		if((charA1  = cipheredText[Ci + 5]) != charR
			&& charA1 != charS && charA1 != charE && charA1 != charG 
			&& charA1 != charU)
		if((charN  = cipheredText[Ci + 6]) != charA1
			&& charN != charS && charN != charE && charN != charG 
			&& charN != charU && charN != charR)
		if((charC  = cipheredText[Ci + 7]) != charN
			&& charC != charS && charC != charE && charC != charG 
			&& charC != charU && charC != charR  && charC != charA1)
		if((charA2 = cipheredText[Ci + 8]) == charA1)
		if(
			// First word
			(Ci == 0 && (lastChar = cipheredText[Ci + 9]) != charA2
				&& lastChar != charS && lastChar != charE && lastChar != charG
				&& lastChar != charU && lastChar != charR  && lastChar != charN
				&& lastChar != charC
			)
			||
			// Last word
			(Ci == lastWord - 1 && (firstChar = cipheredText[Ci - 1]) != charA2
				&& firstChar != charS && firstChar != charE && firstChar != charG
				&& firstChar != charU && firstChar != charR  && firstChar != charN
				&& firstChar != charC
			)
			||
			// Between spaces
			((firstChar = cipheredText[Ci - 1]) == (lastChar = cipheredText[Ci + 9])
				&& firstChar != charA2 && firstChar != charS && firstChar != charE 
				&& firstChar != charG  && firstChar != charU && firstChar != charR  
				&& firstChar != charN  && firstChar != charC
			)
		)
		{
			// Finally it matches
			similarSegurancaWords[indexSimilarSegurancaWords++] = charS;
			similarSegurancaWords[indexSimilarSegurancaWords++] = charE;
			similarSegurancaWords[indexSimilarSegurancaWords++] = charG;
			similarSegurancaWords[indexSimilarSegurancaWords++] = charU;
			similarSegurancaWords[indexSimilarSegurancaWords++] = charR;
			similarSegurancaWords[indexSimilarSegurancaWords++] = charA1;
			similarSegurancaWords[indexSimilarSegurancaWords++] = charN;
			similarSegurancaWords[indexSimilarSegurancaWords++] = charC;
			similarSegurancaWords[indexSimilarSegurancaWords++] = charA2;
			similarSegurancaWords[indexSimilarSegurancaWords++] = (Ci ? firstChar : lastChar);
			similarSegurancaWords[indexSimilarSegurancaWords++] = ',';
		}
	}
	if(indexSimilarSegurancaWords)
		return similarSegurancaWords;
	free(similarSegurancaWords);
	return NULL;
}

/**
 *	Validate both words and return the guessed alphabet
 */
char * validateComputadorVsSeguranca(char * computador, char * seguranca)
{
	int sizeofComputador = strlen(computador), Ci,
		lengthComputador = LENGTH_COMP,
		sizeofSeguranca = strlen(seguranca), Si,
		lengthSeguranca = LENGTH_SEG;

	char * comp, * seg;

	// Position of letters in each word
	char cA = 6, cR = 9, cU = 4, cSpace = 10, // positions of letters A, R, U in computador
		 sA = 5, sR = 4, sU = 3, sSpace = 9;

	char * matchingWords = (char *)malloc(sizeof(char) * (sizeofSeguranca + sizeofComputador + 2));
	char * mw = matchingWords;

	for(Ci = 0; Ci < sizeofComputador; Ci += lengthComputador + 1)
	{
		comp = &computador[Ci];
		for(Si = 0; Si < sizeofSeguranca; Si += lengthSeguranca + 1)
		{
			seg = &seguranca[Si];

			// Begin comparisons
			if(comp[cA] == seg[sA] 
			&& comp[cR] == seg[sR]
			&& comp[cU] == seg[sU]
			&& comp[cSpace] == seg[sSpace])
			{
				strncpy(mw, comp, lengthComputador);
				mw += lengthComputador;
				strncpy(mw, seg, lengthSeguranca);
				mw += lengthSeguranca;
				strncpy(mw, ",", 1);
				mw++;
			}
		}
	}
	if(mw != matchingWords)
		return matchingWords;
	free(matchingWords);
	return NULL;
}

// Matchingwords = computadorseguranca,computadorseguranca
// but, ciphered
void buildInitialCrackAlphabet(char * matchingWords)
{
	int i, sizeofMatching = strlen(matchingWords);
	char * comp, * seg, * mw;
	mw = matchingWords;

	// Letters
	char cC = 0, cO = 1, cM = 2, cP = 3, cU = 4, cT = 5, cA = 6, cD = 7, cR = 9, cSpace = 10, // computador
		 sS = 0, sE = 1, sG = 2, sN = 6;

	for(i = 0; i < sizeofMatching; i += LENGTH_SEG + LENGTH_COMP + 1)
	{
		// Get comp and seg
		comp = mw;
		mw += LENGTH_COMP;
		seg = mw;
		mw += LENGTH_SEG + 1;

		tableAlphabetCracked[comp[cC]] = 'c';
		tableAlphabetCracked[comp[cO]] = 'o';
		tableAlphabetCracked[comp[cM]] = 'm';
		tableAlphabetCracked[comp[cP]] = 'p';
		tableAlphabetCracked[comp[cU]] = 'u';
		tableAlphabetCracked[comp[cT]] = 't';
		tableAlphabetCracked[comp[cA]] = 'a';
		tableAlphabetCracked[comp[cD]] = 'd';
		tableAlphabetCracked[comp[cO]] = 'o';
		tableAlphabetCracked[comp[cR]] = 'r';
		tableAlphabetCracked[comp[cSpace]] = ' ';
		tableAlphabetCracked[seg[sE]] = 'e';
		tableAlphabetCracked[seg[sG]] = 'g';
		tableAlphabetCracked[seg[sN]] = 'n';
		tableAlphabetCracked[seg[sS]] = 's';
	}

	for(i = 0; i < ALPHABET_LENGTH; i++)
		if(tableAlphabetCracked[alphabet[i]] == 0)
			tableAlphabetCracked[alphabet[i]] = '-';
}

char * breakIt(const char * cipheredText, size_t length)
{
	if(!cipheredText)
		return NULL;
	int i;
	char * broken = (char *)malloc(sizeof(char) * length);
	for(i = 0; i < length; i++)
		broken[i] = (tableAlphabetCracked[cipheredText[i]] != '-') ?
					tableAlphabetCracked[cipheredText[i]] :
					'.';
	return broken;
}

void resolveWords(char * broken, size_t length)
{
	int i, j, wordLength = 0;
	char word[128];
	char * basicCmd = "grep -i ";
	char * cmd = (char *)malloc(sizeof(char) * 256);
	char * words;
	for(i = 0; i < length; i++)
	{
		// Capture a word
		wordLength = 0;
		while(i < length && broken[i] != ' ')
			word[wordLength++] = broken[i++];

		//printf("i = %i, wordLength = %i\n", i, wordLength);

		// Check if that word has '.'
		j = wordLength;
		while(--j > -1 && word[j] != '.');

		//printf("j = %i\n", j);
		// Ops! the word seems complete
		if(j == -1 || wordLength < 2)
			continue;

		word[wordLength] = '\0';
		//printf("word = %s\n", word);
		//printf("broken = %s\n", &broken[i]);

		// Build the cmd
		strncpy(cmd, basicCmd, 8);
		strcpy(cmd + 8, "^");
		strcpy(cmd + 9, word);
		strcpy(cmd + 9 + wordLength, "$");
		strcpy(cmd + 10 + wordLength, DICTIONARY);
		words = shell_exec(cmd);
		if(words == NULL || strlen(words) < 2)
			continue;
		
		//printf("words = %s\n", words);

		// Found a match, change the reference in the broken text
		strtolower(words, wordLength);
		strncpy(&broken[i - wordLength], words, wordLength);
	}

	//free(cmd);
}

void printCrackedAlphabet()
{
	printf("Cracked alphabet\n");
	int i;
	for(i = 0; i < ALPHABET_LENGTH; i++)
		printf("%c ", alphabet[i]);
	printf("\n");
	for(i = 0; i < ALPHABET_LENGTH; i++)
		printf("%c ", tableAlphabetCracked[alphabet[i]]);
	printf("\n");
}

char * crackSubstitution(char * cipheredText, size_t length)
{
	if(!cipheredText)
		return NULL;
	// The logic here is to find pattern that match words
	// computador and seguranca

	// Get aproximations of the words
	char * computador = getSimilarComputadorWords(cipheredText, length);
	if(!computador)
		return NULL;

	char * seguranca  = getSimilarSegurancaWords(cipheredText, length);
	if(!seguranca)
		return NULL;

	// Validate each word with another one
	// it means computador has some letters that belongs to seguranca
	if(DEBUG)
		printf("*** ANALISYS: Found computador %s and seguranca %s, trying match: ", computador, seguranca);

	char * matchingWords = validateComputadorVsSeguranca(computador, seguranca);
	if(!matchingWords)
	{
		if(DEBUG)
			printf("They don't match =/\n");
		return NULL;
	}
	if(DEBUG)
		printf("They match =) \n");

	// Build the initial alphabet
	buildInitialCrackAlphabet(matchingWords);

	// Break it using the cracked alphabet
	char * broken = breakIt(cipheredText, length);
	if(!broken)
		return NULL;

	return broken;

	// Resolve accordingly with portuguese dictionary
	resolveWords(broken, length);

	//printf("Computador = %s\n", computador);
	//printf("Seguranca = %s\n", seguranca);
	//printf("matchingWords = %s\n", matchingWords);
	//printf("Broken = %s\n", broken);

	// Check cracked alphabet for a while
	//printCrackedAlphabet();

	// Set memory to be freeeeeee
	//free(broken);
	//free(matchingWords);
	//free(seguranca);
	//free(computador);

	return broken;
}