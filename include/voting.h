#pragma once

// Voting after a meeting. Every badge shows the players by color (plus SKIP),
// you pick with LEFT/RIGHT and cast with A. Votes are broadcast, so every
// badge tallies the same result locally and shows who was ejected -- no
// central counter needed.

#define VOTESTART_MSG "VOTESTART"   // discussion over -> start voting
// a vote is sent as "VOTE:<voterId>:<targetId>", targetId = "SKIP" or a player id

void startVoting();                       // enter the voting phase
bool isVotingActive();
void voteSelect(int dir);                 // LEFT/RIGHT change the selection
void castVote();                          // A: broadcast + record my vote
void handleVoteMessage(const char *msg);  // record a VOTE:/ from anyone
void updateVoting();                      // timer, redraw, tally, result
