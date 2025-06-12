#include <iostream>
#include <vector>
#include <algorithm>
#include <climits>
#include <cmath>

// Constants for piece values
const int PAWN_VALUE = 100;
const int KNIGHT_VALUE = 300;
const int BISHOP_VALUE = 300;
const int ROOK_VALUE = 500;
const int QUEEN_VALUE = 900;
const int KING_VALUE = 20000;

// Enum for pieces
enum Piece { EMPTY, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };
enum Color { WHITE, BLACK };

// Structure to represent a move
struct Move {
    int fromX, fromY, toX, toY;
    Move(int fx = 0, int fy = 0, int tx = 0, int ty = 0) : fromX(fx), fromY(fy), toX(tx), toY(ty) {}
};

// Class to represent the chess board
class ChessBoard {
private:
    std::vector<std::vector<Piece>> board;
    std::vector<std::vector<Color>> colors;
    Color currentPlayer;
    std::vector<Move> moveHistory;
    std::vector<Piece> capturedPieces;

    void initializeBoard();
    void generatePieceMoves(int x, int y, std::vector<Move>& moves);
    void generatePawnMoves(int x, int y, std::vector<Move>& moves);
    void generateKnightMoves(int x, int y, std::vector<Move>& moves);
    void generateBishopMoves(int x, int y, std::vector<Move>& moves);
    void generateRookMoves(int x, int y, std::vector<Move>& moves);
    void generateKingMoves(int x, int y, std::vector<Move>& moves);
    bool isValidMove(int x, int y);
    int getPieceValue(Piece piece);
    char getPieceChar(Piece piece, Color color);

public:
    ChessBoard() : board(8, std::vector<Piece>(8, EMPTY)), colors(8, std::vector<Color>(8, WHITE)), currentPlayer(WHITE) {
        initializeBoard();
    }

    void makeMove(const Move& move);
    void undoMove();
    std::vector<Move> generateMoves();
    int evaluate();
    bool isGameOver();
    Color getCurrentPlayer() const;
    void print();
};

void ChessBoard::initializeBoard() {
    // Set up pawns
    for (int i = 0; i < 8; ++i) {
        board[1][i] = PAWN;
        colors[1][i] = WHITE;
        board[6][i] = PAWN;
        colors[6][i] = BLACK;
    }

    // Set up other pieces
    Piece setupOrder[] = {ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK};
    for (int i = 0; i < 8; ++i) {
        board[0][i] = setupOrder[i];
        colors[0][i] = WHITE;
        board[7][i] = setupOrder[i];
        colors[7][i] = BLACK;
    }
}

void ChessBoard::makeMove(const Move& move) {
    Piece capturedPiece = board[move.toY][move.toX];
    capturedPieces.push_back(capturedPiece);

    board[move.toY][move.toX] = board[move.fromY][move.fromX];
    colors[move.toY][move.toX] = colors[move.fromY][move.fromX];
    board[move.fromY][move.fromX] = EMPTY;
    colors[move.fromY][move.fromX] = WHITE; // Default color, doesn't matter for empty squares

    moveHistory.push_back(move);
    currentPlayer = (currentPlayer == WHITE) ? BLACK : WHITE;
}

void ChessBoard::undoMove() {
    if (moveHistory.empty()) return;

    Move lastMove = moveHistory.back();
    moveHistory.pop_back();

    Piece capturedPiece = capturedPieces.back();
    capturedPieces.pop_back();

    board[lastMove.fromY][lastMove.fromX] = board[lastMove.toY][lastMove.toX];
    colors[lastMove.fromY][lastMove.fromX] = colors[lastMove.toY][lastMove.toX];
    board[lastMove.toY][lastMove.toX] = capturedPiece;
    colors[lastMove.toY][lastMove.toX] = (currentPlayer == WHITE) ? BLACK : WHITE;

    currentPlayer = (currentPlayer == WHITE) ? BLACK : WHITE;
}

std::vector<Move> ChessBoard::generateMoves() {
    std::vector<Move> moves;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            if (colors[y][x] == currentPlayer) {
                generatePieceMoves(x, y, moves);
            }
        }
    }
    return moves;
}

void ChessBoard::generatePieceMoves(int x, int y, std::vector<Move>& moves) {
    switch (board[y][x]) {
        case PAWN: generatePawnMoves(x, y, moves); break;
        case KNIGHT: generateKnightMoves(x, y, moves); break;
        case BISHOP: generateBishopMoves(x, y, moves); break;
        case ROOK: generateRookMoves(x, y, moves); break;
        case QUEEN:
            generateBishopMoves(x, y, moves);
            generateRookMoves(x, y, moves);
            break;
        case KING: generateKingMoves(x, y, moves); break;
        default: break;
    }
}

// Implement generatePawnMoves, generateKnightMoves, generateBishopMoves, generateRookMoves, and generateKingMoves here
void generatePawnMoves(int x, int y, std::vector<Move>& moves) {
        int direction = (currentPlayer == WHITE) ? 1 : -1;
        int startRank = (currentPlayer == WHITE) ? 1 : 6;

        // Move forward
        if (isValidMove(x, y + direction) && board[y + direction][x] == EMPTY) {
            moves.push_back(Move(x, y, x, y + direction));

            // Double move from starting position
            if (y == startRank && isValidMove(x, y + 2 * direction) && board[y + 2 * direction][x] == EMPTY) {
                moves.push_back(Move(x, y, x, y + 2 * direction));
            }
        }

        // Capture diagonally
        for (int dx = -1; dx <= 1; dx += 2) {
            if (isValidMove(x + dx, y + direction) && 
                board[y + direction][x + dx] != EMPTY && 
                colors[y + direction][x + dx] != currentPlayer) {
                moves.push_back(Move(x, y, x + dx, y + direction));
            }
        }

        // TODO: Implement en passant and promotion
    }

    void generateKnightMoves(int x, int y, std::vector<Move>& moves) {
        int knightMoves[8][2] = {{2,1}, {1,2}, {-1,2}, {-2,1}, {-2,-1}, {-1,-2}, {1,-2}, {2,-1}};
        for (auto& move : knightMoves) {
            int newX = x + move[0];
            int newY = y + move[1];
            if (isValidMove(newX, newY) && (board[newY][newX] == EMPTY || colors[newY][newX] != currentPlayer)) {
                moves.push_back(Move(x, y, newX, newY));
            }
        }
    }

    void generateBishopMoves(int x, int y, std::vector<Move>& moves) {
        int directions[4][2] = {{1,1}, {1,-1}, {-1,1}, {-1,-1}};
        for (auto& dir : directions) {
            int newX = x + dir[0];
            int newY = y + dir[1];
            while (isValidMove(newX, newY)) {
                if (board[newY][newX] == EMPTY) {
                    moves.push_back(Move(x, y, newX, newY));
                } else {
                    if (colors[newY][newX] != currentPlayer) {
                        moves.push_back(Move(x, y, newX, newY));
                    }
                    break;
                }
                newX += dir[0];
                newY += dir[1];
            }
        }
    }

    void generateRookMoves(int x, int y, std::vector<Move>& moves) {
        int directions[4][2] = {{0,1}, {1,0}, {0,-1}, {-1,0}};
        for (auto& dir : directions) {
            int newX = x + dir[0];
            int newY = y + dir[1];
            while (isValidMove(newX, newY)) {
                if (board[newY][newX] == EMPTY) {
                    moves.push_back(Move(x, y, newX, newY));
                } else {
                    if (colors[newY][newX] != currentPlayer) {
                        moves.push_back(Move(x, y, newX, newY));
                    }
                    break;
                }
                newX += dir[0];
                newY += dir[1];
            }
        }
    }

    void generateKingMoves(int x, int y, std::vector<Move>& moves) {
        int kingMoves[8][2] = {{1,0}, {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}, {0,-1}, {1,-1}};
        for (auto& move : kingMoves) {
            int newX = x + move[0];
            int newY = y + move[1];
            if (isValidMove(newX, newY) && (board[newY][newX] == EMPTY || colors[newY][newX] != currentPlayer)) {
                moves.push_back(Move(x, y, newX, newY));
            }
        }
    }
bool ChessBoard::isValidMove(int x, int y) {
    return x >= 0 && x < 8 && y >= 0 && y < 8;
}

int ChessBoard::evaluate() {
    int score = 0;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            if (board[y][x] != EMPTY) {
                int pieceValue = getPieceValue(board[y][x]);
                if (colors[y][x] == WHITE) {
                    score += pieceValue;
                } else {
                    score -= pieceValue;
                }
            }
        }
    }
    return score;
}

int ChessBoard::getPieceValue(Piece piece) {
    switch (piece) {
        case PAWN: return PAWN_VALUE;
        case KNIGHT: return KNIGHT_VALUE;
        case BISHOP: return BISHOP_VALUE;
        case ROOK: return ROOK_VALUE;
        case QUEEN: return QUEEN_VALUE;
        case KING: return KING_VALUE;
        default: return 0;
    }
}

bool ChessBoard::isGameOver() {
    return generateMoves().empty();
}

Color ChessBoard::getCurrentPlayer() const {
    return currentPlayer;
}

void ChessBoard::print() {
    std::cout << "  a b c d e f g h" << std::endl;
    for (int y = 7; y >= 0; --y) {
        std::cout << y + 1 << " ";
        for (int x = 0; x < 8; ++x) {
            char piece = getPieceChar(board[y][x], colors[y][x]);
            std::cout << piece << " ";
        }
        std::cout << y + 1 << std::endl;
    }
    std::cout << "  a b c d e f g h" << std::endl;
}

char ChessBoard::getPieceChar(Piece piece, Color color) {
    char pieceChars[] = {'.', 'P', 'N', 'B', 'R', 'Q', 'K'};
    char c = pieceChars[piece];
    return (color == WHITE) ? c : std::tolower(c);
}

int negamax(ChessBoard& board, int depth, int alpha, int beta, Color player) {
    if (depth == 0 || board.isGameOver()) {
        return (player == WHITE) ? board.evaluate() : -board.evaluate();
    }

    std::vector<Move> moves = board.generateMoves();
    int bestScore = INT_MIN;

    for (const auto& move : moves) {
        board.makeMove(move);
        int score = -negamax(board, depth - 1, -beta, -alpha, Color(1 - player));
        board.undoMove();

        bestScore = std::max(bestScore, score);
        alpha = std::max(alpha, score);

        if (alpha >= beta) {
            break;
        }
    }

    return bestScore;
}

Move findBestMove(ChessBoard& board, int depth) {
    std::vector<Move> moves = board.generateMoves();
    Move bestMove;
    int bestScore = INT_MIN;
    Color currentPlayer = board.getCurrentPlayer();

    for (const auto& move : moves) {
        board.makeMove(move);
        int score = -negamax(board, depth - 1, INT_MIN, INT_MAX, Color(1 - currentPlayer));
        board.undoMove();

        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
        }
    }

    return bestMove;
}

int main() {
    ChessBoard board;
    int depth = 4; // Adjust the search depth as needed

    while (!board.isGameOver()) {
        board.print();
        std::cout << (board.getCurrentPlayer() == WHITE ? "White" : "Black") << " to move." << std::endl;

        Move bestMove = findBestMove(board, depth);
        std::cout << "Best move: " << (char)('a' + bestMove.fromX) << (bestMove.fromY + 1) 
                  << " to " << (char)('a' + bestMove.toX) << (bestMove.toY + 1) << std::endl;

        board.makeMove(bestMove);

        std::cout << std::endl;
    }

    board.print();
    std::cout << "Game over. " << (board.getCurrentPlayer() == WHITE ? "Black" : "White") << " wins!" << std::endl;

    return 0;
}