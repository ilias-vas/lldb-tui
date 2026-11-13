#include <chrono>
#include <iostream>
#include <string>
#include <thread>

struct Score {
  int rounds;
  int total;
};

int add_score(int total, int points) {
  // Break here and inspect total, points, updated.
  int updated = total + points;
  return updated;
}

int main(int argc, char **argv) {
  Score score{5, 0};
  for (int round = 1; round <= score.rounds; ++round) {
    score.total = add_score(score.total, round * 10);
    std::cout << "Round " << round << ": " << score.total << std::endl;
  }
  // Optional long-running mode for testing process interrupt / continue.
  if (argc > 1 && std::string(argv[1]) == "--wait") {
    std::cout << "Waiting; interrupt the process to inspect it." << std::endl;
    for (;;)
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  std::cout << "Final score: " << score.total << std::endl;
  return score.total == 150 ? 0 : 1;
}
