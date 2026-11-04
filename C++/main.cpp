#include <bits/stdc++.h>
#include <random>
#include <iostream>
#include <fstream>
#include <string>
using namespace std;

#define N 7906
const int DIM = 300;

typedef struct vec{
    string word;
    float vector[300];
}vec;

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

int main(){
    std::ios_base::sync_with_stdio(false); std::cin.tie(NULL);

vector<vec> Vec;
    unordered_map<string, int> word_to_index;
    int count = 0;
    cout << "Loading the data ...\n" << flush;

    if(!load_binary("data/vectors.bin", Vec, word_to_index, count)){
        // First run (or the .bin is missing/broken): parse the text file once
        cout << "No binary cache found make sure u have the binary file and try again\n" << flush;
        return 0;
    }
    cout << "===ENJOY THE GAME===\n\n" << flush;

// START THE GAME : (unchanged from here on)
    // SELECT THE RANDOM WORD :
    cout << "Type:\n\t>HELP for help\n\t>HINT for hint\n\t>EXIT for exiting\n"; // pick the secret word
    unsigned seed = chrono::high_resolution_clock::now().time_since_epoch().count();
    mt19937 gen(seed);
    uniform_int_distribution<int> distr(0, count-1);
    int secret = distr(gen);   // cout << "Secret: " << Vec[secret].word << "\n";   // debug only
    // STORE THE COSINE SIMILARITIES FOR EASY LOOKUP + BUILD THE RANK SYSTEM :
    vector<float> sims(count);
    for(int i=0;i<count;i++) sims[i] = cosine_similarity(Vec[i].vector, Vec[secret].vector); 
    vector<int> order(count);
    iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int a, int b){ // inbuilt helper function
        if(a==secret) return b != secret;
        if(b==secret) return false;
        return sims[a]>sims[b]; // decreasing order of cosine similarity
    });
    vector<int> rank_of(count);
    for(int r=0;r<count;r++) rank_of[order[r]] = r + 1; // final ranks
    // ACTUALLY START THE GAME : 
    string guess;
    int tries, hints, best_rank = 6700; // 6700 is just a temp value
    tries = hints = 0;
    while(67){
        guess = "NotAWord";
        cout << "Guess a Word : " << flush;
        cin >> guess;
        if(strcmp("RUSHIL",guess.c_str())==0){
            cout << "Secret: " << Vec[secret].word << "\n";
            continue;
        }else if(guess == "HELP"){
            cout << "Guess a word from the dictionary.\n";
            cout << "A lower rank means your guess is semantically closer.\n";
            cout << "Rank 1 is the secret word.\n";
            cout << "Type HINT for hints or EXIT to quit.\n";
        continue;
        }else if(strcmp("EXIT",guess.c_str())==0){
            cout << "The word was : " << Vec[secret].word << "\n";
            cout << "The top 20 words nearest to this word were :\n";
            for(int i=2;i<=21;i++){
                cout << i << " : " << Vec[order[i-1]].word;
                if(i == best_rank) cout << "<-- You were here"; // if the guy comes close (the top 20)
                cout << "\n";
            }
            return 0;
        }else if(strcmp("HINT",guess.c_str())==0){
            if(hints == 0){
                cout << "-----HINT - 1-----\n" << "The word contains " << Vec[secret].word.length() << " letters\n-----Hint - 1-----\n";
                hints ++;
            }else if(hints == 1){
                cout << "-----HINT - 2-----\n" << "The word starts with the letter " << Vec[secret].word[0] << "\n-----Hint - 2-----\n";
                hints ++;
            }else if(hints == 2){
                cout << "-----HINT - 3-----\n" << "The word ends with the letter " << Vec[secret].word[Vec[secret].word.length()-1] << "\n-----Hint - 3-----\n";
                hints ++;
            }else{
                if(best_rank < 50) cout << "Alright thats enough hints for today no more hints now !\n";
                else{
                    uniform_int_distribution<int> hint_dist(2, best_rank-1);
                    int random_rank = hint_dist(gen);
                    cout << "The word at rank " << random_rank << " is " << Vec[order[random_rank - 1]].word << "\n";
                }
                hints++;
            }
            continue;
        }
        auto it = word_to_index.find(guess);
        if(it != word_to_index.end()){
            tries++;
            cout << "Rank : " << rank_of[it->second] << "\n";
            if(best_rank > rank_of[it->second]) best_rank = rank_of[it->second];
            if (it->second == secret) {
                cout << "Congratulations! You found the word in " << tries << " guesses!\n";
                break;
            }
        }else{
            cout << "Word Not found, Try again !\n";
        }
    }

    return 0;
}