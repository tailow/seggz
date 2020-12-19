#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdint>
#include <array>

#include "uci.h"
#include "evaluate.h"
#include "../lib/thc/thc.h"

using namespace std;
using namespace chrono_literals;

enum Type
{
    EXACT,
    UPPERBOUND,
    LOWERBOUND
};

struct Node
{
    uint64_t hash;
    float score;
    thc::Move bestMove;
    uint8_t depth;
    int8_t mate;
    bool isMate;
    Type type;

    Node(uint64_t p_hash, float p_score, thc::Move p_bestMove, uint8_t p_depth, int8_t p_mate, Type p_type)
    {
        hash = p_hash;
        bestMove = p_bestMove;
        depth = p_depth;
        type = p_type;
    }

    Node(float p_score)
    {
        score = p_score;
    }

    Node()
    {
    }
};

int nodes;

// max hash size in mb
const unsigned int MAX_HASH = 1024;

const size_t TT_MAX_SIZE = MAX_HASH * 1000000 / sizeof(Node);

array<Node, TT_MAX_SIZE> transpositionTable;

// get legal moves with move ordering
vector<thc::Move> getLegalMoves(vector<thc::Move> &pv, int &ply, uint64_t &hash, thc::ChessRules &board)
{
    vector<thc::Move> legalMoves;

    board.GenLegalMoveList(legalMoves);

    if (ply >= pv.size())
        return legalMoves;

    vector<float> scores(legalMoves.size(), -100000);

    for (int i = 0; i < legalMoves.size(); i++)
    {
        if (legalMoves[i] == pv[ply])
        {
            swap(legalMoves[i], legalMoves[0]);

            break;
        }
    }

    // selection sort
    for (int j = 1; j < legalMoves.size(); j++)
    {
        for (int i = j; i < legalMoves.size(); i++)
        {
            Node node = transpositionTable[board.Hash64Update(hash, legalMoves[i]) % TT_MAX_SIZE];

            // if stored move score is better
            if (node.score != 0 && -node.score > scores[j])
            {
                scores[j] = -node.score;

                swap(legalMoves[i], legalMoves[j]);
            }
        }
    }

    /*
    if (ply == 0)
    {
        cout << endl;

        for (int i = 0; i < legalMoves.size(); i++)
        {
            cout << legalMoves[i].TerseOut() << ": " << scores[i] << endl;
        }

        cout << endl;
    }
    */

    return legalMoves;
}

// recursive search with alpha-beta pruning
Node negamax(thc::ChessRules &board, uint8_t depth, float alpha, float beta, int color, int ply, vector<thc::Move> &pv, uint64_t &hash)
{
    nodes++;

    Node node;

    if (!uci::searching)
        return node;

    if (depth == 0)
    {
        node.score = color * evaluate(board, node.isMate);

        return node;
    }

    int index = hash % TT_MAX_SIZE;

    // transposition
    if (transpositionTable[index].hash == hash && transpositionTable[index].depth >= depth)
    {
        node = transpositionTable[index];

        if (node.type == EXACT)
        {
            return node;
        }

        else if (node.type == LOWERBOUND)
        {
            alpha = max(alpha, node.score);
        }

        else if (node.type == UPPERBOUND)
        {
            beta = min(beta, node.score);
        }

        if (alpha >= beta)
        {
            return node;
        }
    }

    node.depth = depth;
    node.hash = hash;
    node.score = -1000000;

    double alphaOrig = alpha;

    vector<thc::Move> legalMoves = getLegalMoves(pv, ply, hash, board);

    for (int i = 0; i < legalMoves.size(); i++)
    {
        hash = board.Hash64Update(hash, legalMoves[i]);
        board.PlayMove(legalMoves[i]);

        Node child = negamax(board, depth - 1, -beta, -alpha, -color, ply + 1, pv, hash);

        board.PopMove(legalMoves[i]);
        hash = board.Hash64Update(hash, legalMoves[i]);

        if (-child.score > node.score)
        {
            node.score = -child.score;
            node.bestMove = legalMoves[i];

            if (child.isMate || child.mate != 0)
            {
                node.mate = -child.mate + color;
            }
        }

        alpha = max(alpha, node.score);

        if (alpha >= beta)
        {
            break;
        }
    }

    // store table entry
    if (transpositionTable[hash % TT_MAX_SIZE].depth <= depth && uci::searching)
    {
        if (node.score <= alphaOrig)
        {
            node.type = UPPERBOUND;
        }

        else if (node.score >= beta)
        {
            node.type = LOWERBOUND;
        }

        else
        {
            node.type = EXACT;
        }

        transpositionTable[hash % TT_MAX_SIZE] = node;
    }

    return node;
}

// gets principal variation from transposition table
vector<thc::Move> getPv(uint64_t hash, uint8_t depth, thc::ChessRules board)
{
    vector<thc::Move> pv;

    for (int i = 0; i < depth; i++)
    {
        Node node = transpositionTable[hash % TT_MAX_SIZE];

        if (node.bestMove.TerseOut() != "0000")
        {
            pv.push_back(node.bestMove);

            hash = board.Hash64Update(hash, node.bestMove);

            board.PlayMove(node.bestMove);
        }
    }

    return pv;
}

// searches and prints out best move and stats
void search(thc::ChessRules board, uint8_t maxDepth, uint64_t hash)
{
    uci::searching = true;

    typedef std::chrono::high_resolution_clock Time;
    auto startTime = Time::now();

    vector<thc::Move> pv;
    vector<thc::Move> prevPv;

    Node node;

    for (int depth = 1; depth <= maxDepth; depth++)
    {
        if (uci::searching)
        {
            if (board.white)
                node = negamax(board, depth, -1000000, 1000000, 1, 0, pv, hash);
            else
                node = negamax(board, depth, -1000000, 1000000, -1, 0, pv, hash);

            pv = getPv(hash, depth, board);

            if (uci::searching)
            {
                prevPv = pv;
                string score;

                auto stopTime = Time::now();
                chrono::duration<double> duration = stopTime - startTime;
                double ms = duration.count() * 1000;
                int nps = (int)(nodes / ms * 1000);

                if (node.mate != 0)
                    score = "mate " + to_string((int)ceil(((double)node.mate) / 2));

                else
                    score = "cp " + to_string((int)(node.score * 100));

                cout << "info depth " << depth
                     << " score " << score
                     << " time " << (int)ms
                     << " nodes " << nodes
                     << " nps " << nps
                     << " string pv ";

                for (unsigned int i = 0; i < pv.size(); i++)
                {
                    cout << pv.at(i).TerseOut() << " ";
                }

                cout << endl;
            }
        }
    }

    while (uci::pondering)
    {
        this_thread::sleep_for(0.1s);
    }

    if (prevPv.size() > 0 && !uci::pondering)
    {
        cout << "bestmove " << prevPv.at(0).TerseOut();

        if (prevPv.size() > 1)
            cout << " ponder " << prevPv.at(1).TerseOut();

        cout << endl;
    }

    nodes = 0;

    uci::searching = false;
}
