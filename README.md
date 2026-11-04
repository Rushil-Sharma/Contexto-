## How to run it

```
g++ -O2 -std=c++17 main.cpp -o contexto
.\contexto.exe 
```

## Future plans 
```
C++_PLAN{
    . OPTIMISE{
        : store the vectors.txt (24KB) in a binary file format
    }
    . GAMEPLAY{
        : add the hint feature
        . HINT{
            . add the letters in hint 2 and 3
            . add similar word from 1->current best
        }
    }
    . CHEATS{
        . make the random function more random
    }
}

WEBSITE_PLAN{
    . BACKEND{
        . use C++
    }
}

EXTRA{
    . store cookies
    . store a leaderboard (maybe mogodb or something)
}
```

## C++ implementation:
First I had downloaded the google word2vec from this kaggle website ```https://www.kaggle.com/datasets/leadbest/googlenewsvectorsnegative300```. But as this was a very big folder (~3GB unzipped) I have narrowed down the words (from 3 Million words and phrases to 7950 most common words).