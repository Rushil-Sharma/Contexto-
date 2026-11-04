// contexto.h
// The game logic from C++/main.cpp, with all cin/cout removed.
// Every branch of the original game loop (guess / HELP / HINT / EXIT / RUSHIL)
// is now a method that RETURNS its result instead of printing it,
// so the HTTP server can turn it into JSON.
#ifndef CONTEXTO_H
#define CONTEXTO_H

#include <string>
#include <vector>
#include <unordered_map>
#include <random>
#include <memory>

#define N 7906
const int DIM = 300;

typedef struct vec{
    std::string word;
    float vector[300];
}vec;

// ---- functions copied from C++/main.cpp (unchanged logic) ----
float cosine_similarity(float A[], float B[]);
bool load_text(const std::string& path, std::vector<vec>& Vec, std::unordered_map<std::string,int>& word_to_index, int& count);
bool load_binary(const std::string& path, std::vector<vec>& Vec, std::unordered_map<std::string,int>& word_to_index, int& count);

// The loaded dictionary. Loaded ONCE when the server starts and shared by every game.
struct Dictionary{
    std::vector<vec> Vec;
    std::unordered_map<std::string,int> word_to_index;
    int count = 0;
};

// sims -> order -> rank_of for one secret word. (same code as main.cpp)
// Shared between games that happen to have the same secret.
struct Ranking{
    int secret;
    std::vector<int> order;    // order[r-1]  = index of word with rank r
    std::vector<int> rank_of;  // rank_of[i]  = rank of word i  (secret = 1)
};
std::shared_ptr<const Ranking> build_ranking(const Dictionary& D, int secret);

// Small cache so two games with the same secret don't build the ranking twice.
// Holds weak pointers: a ranking is freed once no game uses it.
class RankingCache{
public:
    explicit RankingCache(const Dictionary& D) : D(D) {}
    std::shared_ptr<const Ranking> get(int secret);
private:
    const Dictionary& D;
    std::unordered_map<int, std::weak_ptr<const Ranking>> cache;
};

// ---- results returned by Game (what used to be printed) ----
enum class GuessStatus { OK, WON, NOT_FOUND, GAME_OVER };
struct GuessResult{
    GuessStatus status;
    std::string word;
    int rank = 0;       // only valid when status is OK / WON
    int tries = 0;
    int best_rank = 0;
};

enum class HintType { LETTER_COUNT, FIRST_LETTER, LAST_LETTER, WORD_AT_RANK, NO_MORE };
struct HintResult{
    HintType type;
    int number;           // which hint this was (1, 2, 3, ...)
    int letters = 0;      // LETTER_COUNT
    char letter = 0;      // FIRST_LETTER / LAST_LETTER
    int rank = 0;         // WORD_AT_RANK
    std::string word;     // WORD_AT_RANK
};

struct TopWord{ int rank; std::string word; bool you_were_here; };
struct ExitResult{
    std::string secret;
    std::vector<TopWord> top;   // ranks 2..21
};

// One running game == one run of the original main() after the data was loaded.
class Game{
public:
    // Picks the secret exactly like main.cpp: mt19937 seeded with `seed`,
    // uniform_int_distribution(0, count-1). The same generator is then kept for hints.
    Game(const Dictionary& D, RankingCache& cache, unsigned seed);

    GuessResult guess(const std::string& guess);   // normal word guess
    HintResult  hint();                            // "HINT"
    ExitResult  exit_game();                       // "EXIT"
    std::string cheat_secret() const;              // "RUSHIL"
    static std::vector<std::string> help_text();   // "HELP"

    bool finished() const { return over; }
    bool won() const { return has_won; }
    int  get_tries() const { return tries; }
    int  get_hints() const { return hints; }
    int  get_best_rank() const { return best_rank; }
    int  secret_index() const { return R->secret; }

private:
    const Dictionary& D;
    std::mt19937 gen;           // same generator as the original (secret pick + HINT 4+)
    std::shared_ptr<const Ranking> R;
    int tries, hints, best_rank;
    bool over = false;          // original: `break` / `return 0` ended the loop
    bool has_won = false;
};

#endif
