#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct commstate {
	/* Commutator pulses */
	int p, j;
	/* Commutator position */
	int commpos;
	/* Commutator's idea of its selections */
	int selections[7];
	/* Trunk state: 0 = open, 1 = closed */
	int state;
};

struct senderstate {
	/* The actual values of the selections */
	int *selections;
	/* The names of the selections, and the pointer to the
	   selection we are making */
	enum selection{OB, OG, IB, IG, FB, FT, FU} selptr;
	/* Skip Office, related to selptr.
	   0 = Office Selections reqd, 2 = No Office Selections */
	int SKO;
	/* Sender's pulse count */
	int pulsecount;
	/* Sender's idea of whether we are finished */
	int complete;
};

int sender(struct commstate* trunk1, struct senderstate* sstate);
void commutator(struct commstate* trunk1, struct senderstate* sstate);
void seltonum(struct senderstate* sstate, struct commstate* trunk1);

int main(int argc, char *argv[])
{
	char dialstring[10] = {0}		/* the full dial string coming from the user */;
	char sta[5] = {0};				/* the last 4 digits of the phone number */
	int res = 0;					/* return state of sender */
	struct commstate* trunk1;
	struct senderstate* sstate;

	trunk1 = calloc(1, sizeof(struct commstate));
	/* Default pulse state is Battery (1) */
	trunk1->p = 1;
	/* This oscillates between 1, 0 to effect rising and falling edges */
	trunk1->j = 0;
	/* Keeping track of where the commutator is. Will be used for
	   converting back to line number in the end */
	trunk1->commpos = 0;

	sstate = calloc(1, sizeof(struct senderstate));
	sstate->selections = calloc(10, sizeof(int)); /* OB, OG, IB, IG, FB, FT, FU */
	
	/* If the user ran with an arg, use that, otherwise don't */
	if (argc <= 1) {
		strncpy(dialstring, "7225971", 7);
	} else {
		strncpy(dialstring, argv[1], sizeof(argv[1]));
	}

	/* Determine whether we were given a short (4) dialstring, or a long (6+1) length
	   dialstring. Long dialstrings must be prepended by the letter "T" to indicate
	   tandem routings, and contain 6 numeric characters. */
	if (strlen(dialstring) > 4) {
		printf("long dialstring\n");
		if ((dialstring[0] == 'T') && (strlen(dialstring) == 7)) {
			printf("Tandem friend detected!\n\n");
			/* Start selections at 0 for OB, OG */
			sstate->SKO = 0;
			/* Pull the OB, OG selections right out of the dialstring characters after the "T"
			   and convert them to integers */
			sstate->selections[0] = dialstring[1] - '0';
			sstate->selections[1] = dialstring[2] - '0';
		}	

		/* j is equal to the length of dialstr */
		int j = strlen(dialstring)-1;

		/* j is now an index to the part of the array containing the char "1" */
		j-=3;	
		
		/* Put the last 4 of dialstring into sta */
		for (int pos = 0; pos < 5; pos++) {
			sta[pos] = dialstring[j];		//look inside dialstring at position of j
			j++;
		}
	} else {
		/* If we have a short (4) dialstring, just throw it into sta without any fanfare
		   and set the SKO value to 2 to start selections after OB, OG */
		printf("short dialstring\n");
		strncpy(sta, dialstring, 4);
		sstate->SKO = 2;
	}

	/* Conver to an integer so we can do math on RP selections */
	int line_int = atoi(sta);
	printf("Line number is: %d\n", line_int);

// Remember to add one to each of these for pulse counting purposes. 
// (the first pulse we receive is the 0th, the second pulse is the 1st...)

	sstate->selections[2] = line_int/2000;				// IB
	sstate->selections[3] = (line_int % 2000)/500;		// IG
	sstate->selections[4] = (line_int % 500)/100;		// FB
	sstate->selections[5] = (line_int % 100)/10;		// FT
	sstate->selections[6] = line_int % 10;				// FU

	printf("Selections are: ");

	for (int i=sstate->SKO; i < 6; i++) {
		printf("%d, ", 	sstate->selections[i]);
	}
	printf("%d", 	sstate->selections[6]);
	printf("\n");

	/*  Use snprintf() to convert the int array back to a char array
	    Thats what Asterisk wants to work with so I'll test it here.
		n.b. we are re-using dialstring[] here */

	/* if SKO is 0 then we are making office selections and need to preserve the "T"
	   char before the dialstring */
	if (sstate->SKO == 0) {
		for (int i = sstate->SKO; i < 7; i++) {
			sprintf(&dialstring[i+1], "%d", sstate->selections[i]);
		}
	} else {
		/* if SKO is not 0, then we aren't making office selections and can just
		   copy the selections into the dialstring */
		for (int i=sstate->SKO; i < 7; i++) {
			sprintf(&dialstring[i], "%d", sstate->selections[i]);
		}
		/* Once we've copied selections into the string, move it to the left, and
		   terminate to get rid of the shit to the right */
		memmove(&dialstring, &dialstring[2], 5);
		dialstring[5] = '\0';
	}
		printf("Char dialstring is: '%s'\n\n", dialstring);
		


	trunk1->state=0;
	sstate->complete=0;
	sstate->selptr=sstate->SKO;
	//printf("trunk open 1\n");

	while(res >= 0) {
		commutator(trunk1, sstate);
		res = sender(trunk1, sstate);
//		sleep(1);
	}

	seltonum(sstate, trunk1);
	free(sstate);
	free(trunk1);
}


int sender(struct commstate* trunk1, struct senderstate* sstate)
{
	/* The plan is to bump the max of selptr +2, and then start at 2, or 0
	   depending on whether we need office selections or not. That way, selptr
	   will always end at the same place, but start at different points.
	   This also keeps the number of selptr at the same selection, so
	   we could even use an enum */
	if ((trunk1->state == 0) && (sstate->selptr < 5)
			&& (sstate->complete == 0)) {
		trunk1->state=1;
		//printf("trunk closed top of sender\n");
	}
	
	if (sstate->complete == 0) {
		switch(trunk1->p) {
			case 0:
				//printf("open\n");
				sstate->pulsecount++;					// count a pulse
				printf("Pulse: %d\n", sstate->pulsecount-1);
				
				if (sstate->pulsecount == sstate->selections[sstate->selptr]+1) {
					printf("       STOP Selection %d\n\n\n", sstate->selptr);
	//				printf("trunk open 2\n");
					sstate->pulsecount=0;		// reset positions and then
					sstate->selptr++;			// look at the next selection
					trunk1->state=0;
					return 0;
				}
				break;
			case 1:
				//printf("closed\n");
				return 0;
				break;
			case 2:
				trunk1->state=0;
				sstate->complete=1;
	//			printf("trunk open final\n");
				printf("REVERSAL\n");
				if (sstate-> selptr >= FU) {
					printf("SELECTIONS COMPLETE!\n");
				} else {
					printf("OVERFLOW!\n");
				}
				sstate->pulsecount=0;
				sstate->selptr=OB;
				return -1;
				break;
		}
		return 0;
	}
}

void commutator(struct commstate* trunk1, struct senderstate* sstate)
{
	int *j = &(trunk1->j);			// init: 0 -- Divides by 2 for pulse edges
	int *i = &(trunk1->commpos);	// init: 0 -- Commutator position
	int *p = &(trunk1->p);			// init: 1 -- Pulse being returned
	int *state = &(trunk1->state);	// init: 0 -- Open, 1=Closed
	int *sel = trunk1->selections;

	if (trunk1->state == 1) {
		if (*j == 0) {
			(*j)++;
		} else {
			*j = 0;
			(*i)++;			// increment *i every other loop
		}

		if (*p == 0) {		// oscillate
			*p = 1;
		} else {
			*p = 0;
		}
	} else {
		sel[sstate->selptr] = *i;
//		printf("Comm selptr: %d  Comm sel: %d\n", sstate->selptr, sel[sstate->selptr]);
		*i=0;				// Reset commutator position
	}

	if ((*i >= 12) || ((sstate->selptr > 6) && (trunk1->state == 0))) {	// overflow or we made all selections
		*p = 2;
		*i = 12;			// Bonk!
	}
}

void seltonum(struct senderstate* sstate, struct commstate* trunk1)
{

/*
   CD-26043-01 page 14, Section 5.5
   Take the T,U digits as they are.
   FB - Set first two digits 00,01,02,03,04
   IG - Add 0, 5, 10, or 15
   IB - Add 0, 20 40, 60, 80

   */

	int lineno[4] = {0};
	int *s = sstate->selections;

	lineno[2] = s[6];      // units
    lineno[1] = s[5];      // tens
    lineno[0] = s[4];      // Final Brush - hundreds



    switch (s[3]) {			// Incoming Group - hundreds
        case 0:
            break;
        case 1:
        case 2:
        case 3:
			lineno[0] = lineno[0] + s[3] * 5;
            break;
        default:
            break;
    }

    switch (s[2]) {			// Incoming Brush - thousands
            case 0:
                break;
            case 1:
            case 2:
            case 3:
            case 4:
                lineno[0] = lineno[0] + s[2] * 20;
                break;
            default:
                break;
    }

    char linestring[5] = {0};

    // OK cool we can print an int
    printf("\nInt of line number: ");
	for (int i=0; i<3; i++) {
        printf("%d", lineno[i]);
    }

	// XXX TODO: make this deal with preceding zeroes

    // But to make it a str, we have to get crafty, because element 0 in the int has 2 digits
    // so we must offset linestring[] an extra step after the first snprintf(), or we'll lose
    // the second digit in the array.
    snprintf(&linestring[0], sizeof(linestring)-1, "%d", lineno[0]);
    snprintf(&linestring[2], sizeof(linestring)-1, "%d", lineno[1]);
    snprintf(&linestring[3], sizeof(linestring)-1, "%d", lineno[2]);

    printf("\nString of line number: %s\n", linestring);

}
