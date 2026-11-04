#include <bits/stdc++.h>
#include <random>
#include <iostream>
#include <fstream>
#include <string>
using namespace std;

#define N 7906

typedef struct vec{
    string word;
    float vector[300];
}vec;

float cosine_similarity(float A[],float B[]){
    // This function calculates the cosine of the angle between 2 vectors
    // by first calculating the dot product between A and B and then dividing 
    // by the square root of their norm
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

int main(){
    std::ios_base::sync_with_stdio(false); std::cin.tie(NULL);

// INITIALISING THE VECTORS :
    ifstream file("data/vectors.txt");
    string line; // reads each line
    vector<vec> Vec(N + 4); // 4 is just a random number required for making sure all the words get read
    unordered_map<string, int> word_to_index;
    word_to_index.reserve(N + 4);
    cout << "Loading the data ...\n"; 
    int count = 0;
    for(int i=0;i<N;i++){
        if (!getline(file,line)) break;
        int position_word_end = line.find("="); // This is the "delimiter" for the seperation of words and vectors
        if (position_word_end == string::npos) continue; // string::npos is a 
        Vec[count].word = line.substr(0,position_word_end);
        word_to_index[Vec[count].word] = count;
        string data_part = line.substr(position_word_end + 1);
        stringstream ss(data_part);
        char dummy; 
        ss >> dummy;
        for(int k=0;k<300;k++){
            ss >> Vec[count].vector[k];
            if (k < 299) {
                ss >> dummy;
            }
        }
        count++;
        cout << "\rProgress : " << i/(float)N*100 << flush;
        cout << "\r%" << flush; 
    }
    cout << "\rFully completed !          \n===ENJOY THE GAME===\n" << flush;
    cout << "\n";
    file.close();
    
// START THE GAME : 
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
    int tries, hints;
    tries = hints = 0;
    while(67){
        guess = "NotAWord";
        cout << "Guess a Word : " << flush;
        cin >> guess;
        if(strcmp("RUSHIL",guess.c_str())==0){
            cout << "Secret: " << Vec[secret].word << "\n";
            continue;
        }else if(strcmp("EXIT",guess.c_str())==0){
            cout << "The word was : " << Vec[secret].word << "\n";
            cout << "The top 20 words nearest to this word were :\n";
            for(int i=1;i<=20;i++){
                cout << i << " : " << Vec[order[i]].word << "\n";
            }
            return 0;
        }else if(strcmp("HINT",guess.c_str())==0){
            if(hints == 0){
                cout << "-----HINT - 1-----\n" << "The word contains " << Vec[secret].word.length() << "\n-----Hint - 1-----\n";
            }else{
                continue;
            }
            continue;
        }
        auto it = word_to_index.find(guess);
        // if(strcmp())
        if(it != word_to_index.end()){
            tries++;
            cout << "Rank : " << rank_of[it->second] << "\n";
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