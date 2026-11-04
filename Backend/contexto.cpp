// contexto.cpp
// Same logic as C++/main.cpp. The only difference is that nothing is read from
// cin or written to cout: results are returned to the HTTP layer instead.
#include "contexto.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <numeric>

using namespace std;

// This function calculates the cosine of the angle between 2 vectors
// by first calculating the dot product between A and B and then dividing
// by the square root of their norm
float cosine_similarity(float A[],float B[]){
    float dot_product, N_A, N_B;
    dot_product = N_A = N_B = 0;
    for(int i=0;i<300;i++){
        dot_product += A[i]*B[i];
        N_A += A[i]*A[i];
        N_B += B[i]*B[i];
    }
    if (N_A==0 || N_B==0) return 0;
    return dot_product/(sqrt(N_A)*sqrt(N_B)); // cos = A.B/(root(A)xroot(B))
}

// a magical number as a magic so that we can figure out that the file is really outs
const char MAGIC[4] = {'6','7','6','7'};

bool load_text(const string& path, vector<vec>& Vec, unordered_map<string,int>& word_to_index, int& count){
    ifstream file(path, ios::binary);
    if(!file) return false;
    string buf((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    const char* p = buf.c_str();
    const char* end = p + buf.size();
    Vec.assign(N + 4, vec());
    word_to_index.clear();
    word_to_index.reserve(N + 4);
    count = 0;
    while(p<end && count<(int)Vec.size()){
        const char* line_end = (const char*) memchr(p,'\n', end-p);
        if(!line_end) line_end = end;
        const char* eq = (const char*)memchr(p, '=', line_end - p);
        if(eq){
            vec& v = Vec[count];
            v.word.assign(p, eq);
            const char* q = eq + 1;
            if(q < line_end && *q == '[') q++; // skip '['
            bool ok = true;
            for(int k=0;k<DIM;k++){
                char* next;
                v.vector[k] = strtof(q, &next); // parse one number
                if(next == q){
                    ok = false;
                    break;
                }
                q = next;
                if(q < line_end && *q == ',') q++; // skip ','
            }
            if(ok){
                word_to_index[v.word] = count;
                count++;
            }
        }
        p = line_end + 1;
    }
    Vec.resize(count);
    if(count <= 0) return 0;
    return count;
}

bool load_binary(const string& path, vector<vec>& Vec, unordered_map<string,int>& word_to_index, int& count){
    ifstream in(path, ios::binary);
    if(!in) return false; // file doesn't exist yet
    char magic[4];
    in.read(magic, 4);
    if(!in || memcmp(magic, MAGIC, 4) != 0) return false;
    int32_t c, d;
    in.read((char*)&c, sizeof c);
    in.read((char*)&d, sizeof d);
    if(!in || d != DIM || c <= 0 || c > 10000000) return false;
    Vec.assign(c, vec());
    word_to_index.clear();
    word_to_index.reserve(c);
    for(int i=0;i<c;i++){
        uint32_t len;
        in.read((char*)&len, sizeof len);
        if(!in || len == 0 || len > 1000) return false;
        Vec[i].word.resize(len);
        in.read(&Vec[i].word[0], len);
        in.read((char*)Vec[i].vector, sizeof(float) * DIM);
        if(!in) return false;
        word_to_index[Vec[i].word] = i;
    }
    count = c;
    return true;
}

// STORE THE COSINE SIMILARITIES FOR EASY LOOKUP + BUILD THE RANK SYSTEM :
// (identical to the block in main.cpp)
shared_ptr<const Ranking> build_ranking(const Dictionary& Dict, int secret){
    const int count = Dict.count;
    vector<vec>& Vec = const_cast<vector<vec>&>(Dict.Vec); // cosine_similarity takes float[] (non-const)
    shared_ptr<Ranking> R = make_shared<Ranking>();
    R->secret = secret;

    vector<float> sims(count);
    for(int i=0;i<count;i++) sims[i] = cosine_similarity(Vec[i].vector, Vec[secret].vector);
    vector<int>& order = R->order;
    order.resize(count);
    iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int a, int b){ // inbuilt helper function
        if(a==secret) return b != secret;
        if(b==secret) return false;
        return sims[a]>sims[b]; // decreasing order of cosine similarity
    });
    vector<int>& rank_of = R->rank_of;
    rank_of.resize(count);
    for(int r=0;r<count;r++) rank_of[order[r]] = r + 1; // final ranks
    return R;
}

shared_ptr<const Ranking> RankingCache::get(int secret){
    auto it = cache.find(secret);
    if(it != cache.end()){
        shared_ptr<const Ranking> alive = it->second.lock();
        if(alive) return alive;
    }
    // drop dead entries now and then so the map doesn't grow forever
    if(cache.size() > 2048){
        for(auto e = cache.begin(); e != cache.end();){
            if(e->second.expired()) e = cache.erase(e); else ++e;
        }
    }
    shared_ptr<const Ranking> fresh = build_ranking(D, secret);
    cache[secret] = fresh;
    return fresh;
}

// SELECT THE RANDOM WORD : (same as main.cpp)
static int pick_secret(mt19937& gen, int count){
    uniform_int_distribution<int> distr(0, count-1);
    return distr(gen);
}

Game::Game(const Dictionary& D, RankingCache& cache, unsigned seed)
    : D(D), gen(seed), R(cache.get(pick_secret(gen, D.count))),
      tries(0), hints(0), best_rank(6700) {} // 6700 is just a temp value

// A normal word typed into "Guess a Word :"
GuessResult Game::guess(const string& guess){
    GuessResult res;
    res.word = guess;
    if(over){
        res.status = GuessStatus::GAME_OVER;
    }else{
        auto it = D.word_to_index.find(guess);
        if(it != D.word_to_index.end()){
            tries++;
            res.rank = R->rank_of[it->second];
            if(best_rank > R->rank_of[it->second]) best_rank = R->rank_of[it->second];
            if (it->second == R->secret) {
                // "Congratulations! You found the word in <tries> guesses!"
                res.status = GuessStatus::WON;
                over = true;      // original: break;
                has_won = true;
            }else{
                res.status = GuessStatus::OK;
            }
        }else{
            // "Word Not found, Try again !"
            res.status = GuessStatus::NOT_FOUND;
        }
    }
    res.tries = tries;
    res.best_rank = best_rank;
    return res;
}

// "HINT"
HintResult Game::hint(){
    const string& secret_word = D.Vec[R->secret].word;
    HintResult h;
    h.number = hints + 1;
    if(hints == 0){
        // The word contains N letters
        h.type = HintType::LETTER_COUNT;
        h.letters = (int)secret_word.length();
        hints ++;
    }else if(hints == 1){
        // The word starts with the letter X
        h.type = HintType::FIRST_LETTER;
        h.letter = secret_word[0];
        hints ++;
    }else if(hints == 2){
        // The word ends with the letter X
        h.type = HintType::LAST_LETTER;
        h.letter = secret_word[secret_word.length()-1];
        hints ++;
    }else{
        if(best_rank < 50){
            // Alright thats enough hints for today no more hints now !
            h.type = HintType::NO_MORE;
        }else{
            uniform_int_distribution<int> hint_dist(2, best_rank-1);
            int random_rank = hint_dist(gen);
            // The word at rank <random_rank> is <word>
            h.type = HintType::WORD_AT_RANK;
            h.rank = random_rank;
            h.word = D.Vec[R->order[random_rank - 1]].word;
            // the revealed word counts as a guess (website rule)
            tries++;
            if(best_rank > random_rank) best_rank = random_rank;
        }
        hints++;
    }
    return h;
}

// "EXIT"
ExitResult Game::exit_game(){
    ExitResult e;
    e.secret = D.Vec[R->secret].word;           // The word was : ...
    // The top 20 words nearest to this word were :
    for(int i=2;i<=21 && i<=D.count;i++){
        TopWord t;
        t.rank = i;
        t.word = D.Vec[R->order[i-1]].word;
        t.you_were_here = (i == best_rank);     // "<-- You were here"
        e.top.push_back(t);
    }
    over = true;                                // original: return 0;
    return e;
}

// "RUSHIL"
string Game::cheat_secret() const{
    return D.Vec[R->secret].word;
}

// "HELP"
vector<string> Game::help_text(){
    return {
        "Guess a word from the dictionary.",
        "A lower rank means your guess is semantically closer.",
        "Rank 1 is the secret word.",
        "Type HINT for hints or EXIT to quit."
    };
}
