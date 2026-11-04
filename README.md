## How to run it

### Web
```
cd Backend
g++ -O2 -std=c++14 server.cpp contexto.cpp http_server.cpp -o contexto_server -lws2_32
.\contexto_server.exe        # http://localhost:8080
```

### CLI:
```
cd C++
g++ -O2 -std=c++17 main.cpp -o contexto
.\contexto.exe 
```

## Future plans 
```
C++_PLAN{
    . OPTIMISE{
        : store the vectors.txt (24KB) in a binary file format[
            DONE.
            24KB -> 9KB
            in vector.bin
        ]
    }
    . GAMEPLAY{
        : add the hint feature
        . HINT{
            : add hint 1[
                DONE.
                no of letter in the secret word
            ]
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