#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <sstream>
#include <chrono>
#include <atomic>


    enum Piece { EMPTY, WPAWN, WROOK, WKNIGHT, WBISHOP, WQUEEN, WKING, BPAWN, BROOK, BKNIGHT, BBISHOP, BQUEEN, BKING };

    struct Move {
        int fromRow, fromCol, toRow, toCol;
        bool isEnPassant = false;
        bool isCastling = false;
        bool isPromotion = false;
        Piece promotionPiece = EMPTY;
        Piece capturedPiece = EMPTY;
        int moveScore = 0;  // Heuristic score for move ordering
    };

    struct TTEntry {
        uint64_t hash;    // The Zobrist hash key
        int depth;        // Depth of the search when this entry was stored
        int score;        // Evaluation score
        Move bestMove;    // Best move found from this position
        enum Flag { EXACT, LOWERBOUND, UPPERBOUND } flag;
    };
    std::unordered_map<uint64_t, TTEntry> transpositionTable;
    uint64_t zobristTable[13][8][8];  // 13 piece types (including EMPTY) for each square
    uint64_t zobristBlackToMove;      // To represent side to move
    uint64_t zobristCastlingRights[16]; // 16 possible castling rights combinations
    uint64_t zobristEnPassant[8];     // For en passant column (if any)
    uint64_t currentHash;             // Current Zobrist hash of the board

    void initializeZobrist() {
        std::mt19937_64 rng_zobrist(std::time(0));
        std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);

        for (int piece = 0; piece <= BKING; ++piece) {
            for (int row = 0; row < 8; ++row) {
                for (int col = 0; col < 8; ++col) {
                    zobristTable[piece][row][col] = dist(rng_zobrist);
                }
            }
        }

        zobristBlackToMove = dist(rng_zobrist);

        for (int i = 0; i < 16; ++i) {
            zobristCastlingRights[i] = dist(rng_zobrist);
        }

        for (int i = 0; i < 8; ++i) {
            zobristEnPassant[i] = dist(rng_zobrist);
        }
    }


    std::vector<Piece> capturedPieces;
    std::vector<int> enPassantHistory;
    std::vector<bool> castlingRights;


    const int pawnTable[8][8] = {
            {  0,  0,  0,  0,  0,  0,  0,  0 },
            { 50, 50, 50, 50, 50, 50, 50, 50 },
            { 10, 10, 20, 30, 30, 20, 10, 10 },
            {  0,  0,  0, 25, 25,  0,  0,  0 },
            {  0,  0,  0, 25, 25,  0,  0,  0 },
            { 10, 10, 20, 30, 30, 20, 10, 10 },
            { 50, 50, 50, 50, 50, 50, 50, 50 },
            {  0,  0,  0,  0,  0,  0,  0,  0 }
    };
    const int knightTable[8][8] = {
            { -50, -40, -30, -30, -30, -30, -40, -50 },
            { -40, -20,   0,   0,   0,   0, -20, -40 },
            { -30,   0,  10,  15,  15,  10,   0, -30 },
            { -30,   5,  15,  20,  20,  15,   5, -30 },
            { -30,   0,  15,  20,  20,  15,   0, -30 },
            { -30,   5,  10,  15,  15,  10,   5, -30 },
            { -40, -20,   0,   5,   5,   0, -20, -40 },
            { -50, -40, -30, -30, -30, -30, -40, -50 }
    };
    const int bishopTable[8][8] = {
            { -20, -10, -10, -10, -10, -10, -10, -20 },
            { -10,   5,   0,   0,   0,   0,   5, -10 },
            { -10,  10,  10,  10,  10,  10,  10, -10 },
            { -10,   0,  10,  10,  10,  10,   0, -10 },
            { -10,   5,  10,  10,  10,  10,   5, -10 },
            { -10,   0,   5,  10,  10,   5,   0, -10 },
            { -10,   5,   0,   0,   0,   0,   5, -10 },
            { -20, -10, -10, -10, -10, -10, -10, -20 }
    };
    const int rookTable[8][8] = {
            {  0,  0,  5, 10, 10,  5,  0,  0 },
            {  0,  0,  5, 10, 10,  5,  0,  0 },
            {  0,  0,  5, 10, 10,  5,  0,  0 },
            {  0,  0,  5, 10, 10,  5,  0,  0 },
            {  0,  0,  5, 10, 10,  5,  0,  0 },
            {  0,  0,  5, 10, 10,  5,  0,  0 },
            { 25, 25, 25, 25, 25, 25, 25, 25 },
            {  0,  0,  0,  5,  5,  0,  0,  0 }
    };
    const int queenTable[8][8] = {
            { -20, -10, -10,  -5,  -5, -10, -10, -20 },
            { -10,   0,   0,   0,   0,   0,   0, -10 },
            { -10,   0,   5,   5,   5,   5,   0, -10 },
            {  -5,   0,   5,   5,   5,   5,   0,  -5 },
            {  -5,   0,   5,   5,   5,   5,   0,  -5 },
            { -10,   0,   5,   5,   5,   5,   0, -10 },
            { -10,   0,   0,   0,   0,   0,   0, -10 },
            { -20, -10, -10,  -5,  -5, -10, -10, -20 }
    };
    const int whiteKingTableEarly[8][8] = {
            {  20,  30,  10,   0,   0,  10,  30,  20 },
            {  20,  20,   0,   0,   0,   0,  20,  20 },
            {  -5, -10, -10, -10, -10, -10, -10,  -5 },
            { -20, -20, -20, -20, -20, -20, -20, -20 },
            { -30, -30, -30, -30, -30, -30, -30, -30 },
            { -30, -30, -30, -30, -30, -30, -30, -30 },
            { -30, -30, -30, -30, -30, -30, -30, -30 },
            { -30, -30, -30, -30, -30, -30, -30, -30 }
    };
    const int whiteKingTableLate[8][8] = {
            { -50, -40, -30, -20, -20, -30, -40, -50 },
            { -30, -20, -10,   0,   0, -10, -20, -30 },
            { -30, -10,  20,  30,  30,  20, -10, -30 },
            { -30, -10,  30,  40,  40,  30, -10, -30 },
            { -30, -10,  30,  40,  40,  30, -10, -30 },
            { -30, -10,  20,  30,  30,  20, -10, -30 },
            { -30, -30,   0,   0,   0,   0, -30, -30 },
            { -50, -30, -30, -30, -30, -30, -30, -50 }
    };
    const int blackKingTableEarly[8][8] = {
            { -30, -30, -30, -30, -30, -30, -30, -30 },
            { -30, -30, -30, -30, -30, -30, -30, -30 },
            { -30, -30, -30, -30, -30, -30, -30, -30 },
            { -20, -20, -20, -20, -20, -20, -20, -20 },
            {  -5, -10, -10, -10, -10, -10, -10,  -5 },
            {  20,  20,   0,   0,   0,   0,  20,  20 },
            {  20,  30,  10,   0,   0,  10,  30,  20 },
            {  20,  30,  10,   0,   0,  10,  30,  20 }
    };
    const int blackKingTableLate[8][8] = {
            { -50, -30, -30, -30, -30, -30, -30, -50 },
            { -30, -30,   0,   0,   0,   0, -30, -30 },
            { -30, -10,  20,  30,  30,  20, -10, -30 },
            { -30, -10,  30,  40,  40,  30, -10, -30 },
            { -30, -10,  30,  40,  40,  30, -10, -30 },
            { -30, -10,  20,  30,  30,  20, -10, -30 },
            { -30, -20, -10,   0,   0, -10, -20, -30 },
            { -50, -40, -30, -20, -20, -30, -40, -50 }
    };


    class ChessEngine {

    public:
        std::vector<Move> moveHistory; // To keep track of moves made
        // In your ChessEngine class
        uint64_t nodesSearched;
        std::chrono::steady_clock::time_point searchStartTime;

        std::vector<std::vector<Piece>> board;
        bool isWhiteTurn;
        std::mt19937 rng;
        bool whiteKingMoved = false, blackKingMoved = false;
        bool whiteKingsideRookMoved = false, whiteQueensideRookMoved = false;
        bool blackKingsideRookMoved = false, blackQueensideRookMoved = false;
        int enPassantCol = -1;
        bool userIsWhite; // New flag to store the player's side


        ChessEngine() : board(8, std::vector<Piece>(8, EMPTY)), isWhiteTurn(true), userIsWhite(true) {
            initializeBoard();
            rng.seed(std::time(0));
            nodesSearched = 0;
        }
        int getCastlingRightsKey() {
            int key = 0;
            if (!whiteKingMoved) {
                if (!whiteKingsideRookMoved) key |= 1;   // White kingside castling right
                if (!whiteQueensideRookMoved) key |= 2;  // White queenside castling right
            }
            if (!blackKingMoved) {
                if (!blackKingsideRookMoved) key |= 4;   // Black kingside castling right
                if (!blackQueensideRookMoved) key |= 8;  // Black queenside castling right
            }
            return key;
        }

        void initializeBoard() {
            // Set up the initial board state
            for (int i = 0; i < 8; ++i) {
                board[1][i] = WPAWN;
                board[6][i] = BPAWN;
            }

            board[0] = {WROOK, WKNIGHT, WBISHOP, WQUEEN, WKING, WBISHOP, WKNIGHT, WROOK};
            board[7] = {BROOK, BKNIGHT, BBISHOP, BQUEEN, BKING, BBISHOP, BKNIGHT, BROOK};

    //        board = std::vector<std::vector<Piece>>(8, std::vector<Piece>(8, EMPTY));
    //
    //        // Set the pieces on the board as per the given configuration
    //        board[7] = {BROOK, EMPTY, EMPTY, EMPTY, BKING, EMPTY, EMPTY, BROOK}; // Row 8 (index 7)
    //        board[6] = {BPAWN, EMPTY, WROOK, EMPTY, EMPTY, EMPTY, BPAWN, BPAWN}; // Row 7 (index 6)
    //        board[5] = {EMPTY, EMPTY, WPAWN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY}; // Row 6 (index 5)
    //        board[4] = {EMPTY, EMPTY, EMPTY, EMPTY, BPAWN, EMPTY, EMPTY, EMPTY}; // Row 5 (index 4)
    //        board[3] = {EMPTY, EMPTY, WBISHOP, BPAWN, EMPTY, EMPTY, WPAWN, EMPTY}; // Row 4 (index 3)
    //        board[2] = {WBISHOP, EMPTY, EMPTY, EMPTY, EMPTY, WPAWN, EMPTY, EMPTY}; // Row 3 (index 2)
    //        board[1] = {WPAWN, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, WPAWN}; // Row 2 (index 1)
    //        board[0] = {EMPTY, BBISHOP, EMPTY, EMPTY, WKING, EMPTY, EMPTY, WROOK}; // Row 1 (index 0)
            // Initialize Zobrist hashing
            initializeZobrist();

            // Compute the initial hash
            currentHash = 0;
            for (int row = 0; row < 8; ++row) {
                for (int col = 0; col < 8; ++col) {
                    Piece piece = board[row][col];
                    if (piece != EMPTY) {
                        currentHash ^= zobristTable[piece][row][col];
                    }
                }
            }

            // Include side to move
            if (!isWhiteTurn) {
                currentHash ^= zobristBlackToMove;
            }

            // Include castling rights
            currentHash ^= zobristCastlingRights[getCastlingRightsKey()];

            // Include en passant
            if (enPassantCol != -1) {
                currentHash ^= zobristEnPassant[enPassantCol];
            }
        }

        void pickSide() {
            std::string choice;
            while (true) {
                std::cout << "Do you want to play as white or black? (w/b): ";
                std::cin >> choice;
                if (choice == "w" || choice == "W") {
                    userIsWhite = true;
                    break;
                } else if (choice == "b" || choice == "B") {
                    userIsWhite = false;
                    break;
                } else {
                    std::cout << "Invalid choice. Please enter 'w' for white or 'b' for black." << std::endl;
                }
            }
        }

        bool isHumanTurn() {
            // Determine if it's the human player's turn based on their choice and current turn
            return (isWhiteTurn && userIsWhite) || (!isWhiteTurn && !userIsWhite);
        }

        void printBoard() {
            std::cout << "    a   b   c   d   e   f   g   h\n"; // Column labels
            std::cout << "  +---+---+---+---+---+---+---+---+\n"; // Top border
            for (int i = 7; i >= 0; --i) {
                std::cout << i + 1 << " "; // Row label
                for (int j = 0; j < 8; ++j) {
                    char piece = ' ';
                    switch (board[i][j]) {
                        case WPAWN: piece = 'P'; break;
                        case WROOK: piece = 'R'; break;
                        case WKNIGHT: piece = 'N'; break;
                        case WBISHOP: piece = 'B'; break;
                        case WQUEEN: piece = 'Q'; break;
                        case WKING: piece = 'K'; break;
                        case BPAWN: piece = 'P'; break;
                        case BROOK: piece = 'R'; break;
                        case BKNIGHT: piece = 'N'; break;
                        case BBISHOP: piece = 'B'; break;
                        case BQUEEN: piece = 'Q'; break;
                        case BKING: piece = 'K'; break;
                        default: break;
                    }

                    if (board[i][j] >= BPAWN) {
                        std::cout << "| \033[1;31m" << piece << "\033[0m "; // Red pieces for black
                    } else {
                        std::cout << "| " << piece << " "; // Normal pieces for white
                    }
                }
                std::cout << "| " << i + 1 << "\n"; // Row label at the end of the row
                std::cout << "  +---+---+---+---+---+---+---+---+\n"; // Horizontal grid line
            }
            std::cout << "    a   b   c   d   e   f   g   h\n"; // Column labels again at the bottom
        }


        bool isValidMove(Move& move) {
            if (move.fromRow < 0 || move.fromRow >= 8 || move.fromCol < 0 || move.fromCol >= 8 ||
                move.toRow < 0 || move.toRow >= 8 || move.toCol < 0 || move.toCol >= 8) {
                return false;
            }

            Piece piece = board[move.fromRow][move.fromCol];
            if (piece == EMPTY) return false;

            bool isWhitePiece = (piece <= WKING);
            if (isWhiteTurn != isWhitePiece) return false;

            // Cannot capture own piece
            if (board[move.toRow][move.toCol] != EMPTY && isWhitePiece == (board[move.toRow][move.toCol] <= WKING)) {
                return false;
            }

            bool isLegalPieceMove;
            switch (piece) {
                case WPAWN: case BPAWN:
                    isLegalPieceMove = isValidPawnMove(move);
                    break;
                case WROOK: case BROOK:
                    isLegalPieceMove = isValidRookMove(move);
                    break;
                case WKNIGHT: case BKNIGHT:
                    isLegalPieceMove = isValidKnightMove(move);
                    break;
                case WBISHOP: case BBISHOP:
                    isLegalPieceMove = isValidBishopMove(move);
                    break;
                case WQUEEN: case BQUEEN:
                    isLegalPieceMove = isValidQueenMove(move);
                    break;
                case WKING: case BKING:
                    isLegalPieceMove = isValidKingMove(move);
                    break;
                default:
                    return false;
            }

            if (!isLegalPieceMove) return false;

            // **New Code Starts Here**

            // Simulate the move
    //        ChessEngine tempGame = *this;
    //        tempGame.makeMove(move);
    //        tempGame.isWhiteTurn = !tempGame.isWhiteTurn;
            makeMove(move);
            // Check if the king is in check after the move
            isWhiteTurn = !isWhiteTurn;
            if (isInCheck()) {
                isWhiteTurn = !isWhiteTurn;
                undoMove(move);
                return false;
            }
            isWhiteTurn = !isWhiteTurn;
            undoMove(move);
            // **New Code Ends Here**

            return true;
        }

        bool isValidPawnMove(Move& move) {
            int direction = (board[move.fromRow][move.fromCol] == WPAWN) ? 1 : -1;
            int startRow = (board[move.fromRow][move.fromCol] == WPAWN) ? 1 : 6;

            // Move forward
            if (move.fromCol == move.toCol && board[move.toRow][move.toCol] == EMPTY) {
                if (move.toRow - move.fromRow == direction) {
                    move.isPromotion = (move.toRow == 0 || move.toRow == 7);
                    return true;
                }
                if (move.fromRow == startRow && move.toRow - move.fromRow == 2 * direction &&
                    board[move.fromRow + direction][move.fromCol] == EMPTY) {
                    return true;
                }
            }

            // Capture
            if (std::abs(move.toCol - move.fromCol) == 1 && move.toRow - move.fromRow == direction) {
                if (board[move.toRow][move.toCol] != EMPTY) {
                    move.isPromotion = (move.toRow == 0 || move.toRow == 7);
                    return true;
                }
                // En passant
                if (((move.toRow == 2 && !isWhiteTurn) || (move.toRow == 5 && isWhiteTurn)) && move.toCol == enPassantCol) {
                    move.isEnPassant = true;
                    return true;
                }
            }

            return false;
        }
        bool isLateGame() {
            int whiteQueens = 0, blackQueens = 0;
            int whitePieces = 0, blackPieces = 0;

            for (int i = 0; i < 8; ++i) {
                for (int j = 0; j < 8; ++j) {
                    Piece piece = board[i][j];
                    switch (piece) {
                        case WQUEEN:
                            whiteQueens++;
                            break;
                        case BQUEEN:
                            blackQueens++;
                            break;
                        case WPAWN: case WROOK: case WKNIGHT: case WBISHOP:
                            whitePieces++;
                            break;
                        case BPAWN: case BROOK: case BKNIGHT: case BBISHOP:
                            blackPieces++;
                            break;
                        default:
                            break;
                    }
                }
            }

            // Late game condition: no queens on the board or very low total material
            if (whiteQueens == 0 && blackQueens == 0) {
                return true;
            }

            // Another late game condition: low number of total remaining pieces (excluding pawns)
            int totalPieces = whitePieces + blackPieces;
            if (totalPieces <= 6) {  // You can tweak this threshold based on testing
                return true;
            }

            return false;
        }


        bool isValidRookMove(Move& move) {
            if (move.fromRow != move.toRow && move.fromCol != move.toCol) return false;
            return !isPieceBetween(move);
        }

        bool isValidKnightMove(Move& move) {
            int rowDiff = std::abs(move.toRow - move.fromRow);
            int colDiff = std::abs(move.toCol - move.fromCol);
            return (rowDiff == 2 && colDiff == 1) || (rowDiff == 1 && colDiff == 2);
        }

        bool isValidBishopMove(Move& move) {
            if (std::abs(move.toRow - move.fromRow) != std::abs(move.toCol - move.fromCol)) return false;
            return !isPieceBetween(move);
        }

        bool isValidQueenMove(Move& move) {
            return isValidRookMove(move) || isValidBishopMove(move);
        }

        bool isValidKingMove(Move& move) {
            int rowDiff = std::abs(move.toRow - move.fromRow);
            int colDiff = std::abs(move.toCol - move.fromCol);

            // Normal king move
            if (rowDiff <= 1 && colDiff <= 1) return true;

            // Castling
            if (rowDiff == 0 && colDiff == 2) {
                bool isWhiteKing = (board[move.fromRow][move.fromCol] == WKING);
                if ((isWhiteKing && whiteKingMoved) || (!isWhiteKing && blackKingMoved)) return false;

                // Kingside castling
                if (move.toCol > move.fromCol) {
                    if ((isWhiteKing && whiteKingsideRookMoved) || (!isWhiteKing && blackKingsideRookMoved)) return false;
                    if (board[move.fromRow][5] != EMPTY || board[move.fromRow][6] != EMPTY) return false;
                    if (isUnderAttack(move.fromRow, 4, isWhiteTurn) || isUnderAttack(move.fromRow, 5, isWhiteTurn) || isUnderAttack(move.fromRow, 6, isWhiteTurn)) return false;
                }
                    // Queenside castling
                else {
                    if ((isWhiteKing && whiteQueensideRookMoved) || (!isWhiteKing && blackQueensideRookMoved)) return false;
                    if (board[move.fromRow][1] != EMPTY || board[move.fromRow][2] != EMPTY || board[move.fromRow][3] != EMPTY) return false;
                    if (isUnderAttack(move.fromRow, 4, isWhiteTurn) || isUnderAttack(move.fromRow, 3, isWhiteTurn) || isUnderAttack(move.fromRow, 2, isWhiteTurn)) return false;
                }
                move.isCastling = true;
                return true;
            }
            return false;
        }

        bool isPieceBetween(Move& move) {
            int rowStep = (move.toRow > move.fromRow) ? 1 : (move.toRow < move.fromRow) ? -1 : 0;
            int colStep = (move.toCol > move.fromCol) ? 1 : (move.toCol < move.fromCol) ? -1 : 0;
            int row = move.fromRow + rowStep, col = move.fromCol + colStep;

            while (row != move.toRow || col != move.toCol) {
                if (board[row][col] != EMPTY) return true;
                row += rowStep;
                col += colStep;
            }
            return false;
        }

        bool isUnderAttack(int row, int col, bool forWhite) {
            for (int i = 0; i < 8; ++i) {
                for (int j = 0; j < 8; ++j) {
                    Piece attacker = board[i][j];
                    if (attacker == EMPTY) continue;

                    bool attackerIsWhite = (attacker <= WKING);
                    if (attackerIsWhite != forWhite) {
                        Move move = {i, j, row, col};
                        if (isLegalPieceMove(attacker, move)) {
                            if (board[move.toRow][move.toCol] == EMPTY || (board[move.toRow][move.toCol] <= WKING) != attackerIsWhite) {
                                return true;
                            }
                        }
                    }
                }
            }
            return false;
        }



        // void makeMove(Move& move) {
        //     Piece movedPiece = board[move.fromRow][move.fromCol];
        //     move.capturedPiece = board[move.toRow][move.toCol];  // Store captured piece

        //     // Update hash: Remove the piece from the from-square
        //     currentHash ^= zobristTable[movedPiece][move.fromRow][move.fromCol];

        //     // Update hash: Remove the captured piece from the to-square (if any)
        //     if (move.capturedPiece != EMPTY) {
        //         currentHash ^= zobristTable[move.capturedPiece][move.toRow][move.toCol];
        //     }

        //     // Update en passant hash (if applicable)
        //     if (enPassantCol != -1) {
        //         currentHash ^= zobristEnPassant[enPassantCol];
        //     }

        //     enPassantHistory.push_back(enPassantCol);
        //     enPassantCol = -1;

        //     board[move.toRow][move.toCol] = movedPiece;
        //     board[move.fromRow][move.fromCol] = EMPTY;

        //     // Update hash: Add the piece to the to-square
        //     currentHash ^= zobristTable[movedPiece][move.toRow][move.toCol];


        //     capturedPieces.push_back(move.capturedPiece);  // Keep track of captured pieces
        //     //enPassantHistory.push_back(enPassantCol);      // Keep track of enPassantCol
        //     castlingRights.push_back(whiteKingMoved);
        //     castlingRights.push_back(blackKingMoved);
        //     castlingRights.push_back(whiteKingsideRookMoved);
        //     castlingRights.push_back(whiteQueensideRookMoved);
        //     castlingRights.push_back(blackKingsideRookMoved);
        //     castlingRights.push_back(blackQueensideRookMoved);

        //     //enPassantCol = -1;

        //     // Handle special moves
        //     if (move.isEnPassant) {
        //         board[move.fromRow][move.toCol] = EMPTY;
        //     } else if (move.isCastling) {
        //         int rookFromCol = (move.toCol > move.fromCol) ? 7 : 0;
        //         int rookToCol = (move.toCol > move.fromCol) ? 5 : 3;
        //         board[move.toRow][rookToCol] = board[move.toRow][rookFromCol];
        //         board[move.toRow][rookFromCol] = EMPTY;
        //     } else if (move.isPromotion) {
        //         board[move.toRow][move.toCol] = move.promotionPiece;
        //     }

        //     // **Check if a pawn moved two squares forward and update enPassantCol**
        //     if (movedPiece == WPAWN && move.fromRow == 1 && move.toRow == 3) {
        //         enPassantCol = move.fromCol; // White pawn moved two spaces
        //     } else if (movedPiece == BPAWN && move.fromRow == 6 && move.toRow == 4) {
        //         enPassantCol = move.fromCol; // Black pawn moved two spaces
        //     }
        //     // Update castling rights hash
        //     int castlingKeyBefore = getCastlingRightsKey();

        //     // Update castling rights
        //     if (movedPiece == WKING) whiteKingMoved = true;
        //     if (movedPiece == BKING) blackKingMoved = true;
        //     if (movedPiece == WROOK) {
        //         if (move.fromCol == 0) whiteQueensideRookMoved = true;
        //         if (move.fromCol == 7) whiteKingsideRookMoved = true;
        //     }
        //     if (movedPiece == BROOK) {
        //         if (move.fromCol == 0) blackQueensideRookMoved = true;
        //         if (move.fromCol == 7) blackKingsideRookMoved = true;
        //     }
        //     int castlingKeyAfter = getCastlingRightsKey();
        //     if (castlingKeyBefore != castlingKeyAfter) {
        //         currentHash ^= zobristCastlingRights[castlingKeyBefore];
        //         currentHash ^= zobristCastlingRights[castlingKeyAfter];
        //     }
        //     // XOR side to move
        //     currentHash ^= zobristBlackToMove;
        //     isWhiteTurn = !isWhiteTurn;
        // }
        // void undoMove(Move& move) {
        //     isWhiteTurn = !isWhiteTurn;  // Toggle back the turn
        //     // XOR side to move (since we toggled isWhiteTurn)
        //     currentHash ^= zobristBlackToMove;

        //     // Restore enPassantCol
        //     int enPassantColAfter = enPassantCol;
        //     enPassantCol = enPassantHistory.back();
        //     enPassantHistory.pop_back();
        //     int enPassantColBefore = enPassantCol;

        //     // Update en passant hash
        //     if (enPassantColAfter != -1) {
        //         currentHash ^= zobristEnPassant[enPassantColAfter];
        //     }
        //     if (enPassantColBefore != -1) {
        //         currentHash ^= zobristEnPassant[enPassantColBefore];
        //     }

        //     // Restore castling rights
        //     int castlingKeyAfter = getCastlingRightsKey();
        //     // Restore castling rights
        //     blackQueensideRookMoved = castlingRights.back(); castlingRights.pop_back();
        //     blackKingsideRookMoved = castlingRights.back(); castlingRights.pop_back();
        //     whiteQueensideRookMoved = castlingRights.back(); castlingRights.pop_back();
        //     whiteKingsideRookMoved = castlingRights.back(); castlingRights.pop_back();
        //     blackKingMoved = castlingRights.back(); castlingRights.pop_back();
        //     whiteKingMoved = castlingRights.back(); castlingRights.pop_back();
        //     int castlingKeyBefore = getCastlingRightsKey();

        //     // Update castling rights hash
        //     if (castlingKeyBefore != castlingKeyAfter) {
        //         currentHash ^= zobristCastlingRights[castlingKeyAfter];
        //         currentHash ^= zobristCastlingRights[castlingKeyBefore];
        //     }
        //     // Restore enPassantCol
        //     //enPassantCol = enPassantHistory.back(); enPassantHistory.pop_back();

        //     // Undo special moves
        //     if (move.isPromotion) {
        //         // Update hash: Remove the promoted piece from the to-square
        //         currentHash ^= zobristTable[board[move.toRow][move.toCol]][move.toRow][move.toCol];

        //         // Revert to pawn
        //         board[move.toRow][move.toCol] = isWhiteTurn ? WPAWN : BPAWN;

        //         // Update hash: Add the pawn back to the to-square
        //         currentHash ^= zobristTable[board[move.toRow][move.toCol]][move.toRow][move.toCol];
        //     } else if (move.isEnPassant) {
        //         int capturedPawnRow = isWhiteTurn ? move.toRow - 1 : move.toRow + 1;
        //         Piece capturedPawn = isWhiteTurn ? BPAWN : WPAWN;
        //         board[capturedPawnRow][move.toCol] = capturedPawn;

        //         // Update hash for the restored pawn
        //         currentHash ^= zobristTable[capturedPawn][capturedPawnRow][move.toCol];
        //     } else if (move.isCastling) {
        //         int rookFromCol = (move.toCol > move.fromCol) ? 7 : 0;
        //         int rookToCol = (move.toCol > move.fromCol) ? 5 : 3;

        //         // Move the rook back to its original position
        //         Piece rook = board[move.toRow][rookToCol];
        //         board[move.toRow][rookFromCol] = rook;
        //         board[move.toRow][rookToCol] = EMPTY;

        //         // Update hash for rook move
        //         currentHash ^= zobristTable[rook][move.toRow][rookToCol];   // Remove rook from rookToCol
        //         currentHash ^= zobristTable[rook][move.toRow][rookFromCol]; // Place rook back to rookFromCol
        //     }
        //     // Undo the move
        //     Piece movedPiece = board[move.toRow][move.toCol];

        //     // Update hash: Remove the moved piece from the to-square
        //     currentHash ^= zobristTable[movedPiece][move.toRow][move.toCol];

        //     // Update hash: Place the moved piece back to the from-square
        //     currentHash ^= zobristTable[movedPiece][move.fromRow][move.fromCol];

        //     board[move.fromRow][move.fromCol] = movedPiece;

        //     // Restore the captured piece (if any)
        //     if (move.capturedPiece != EMPTY) {
        //         board[move.toRow][move.toCol] = move.capturedPiece;

        //         // Update hash for the restored captured piece
        //         currentHash ^= zobristTable[move.capturedPiece][move.toRow][move.toCol];
        //     } else {
        //         board[move.toRow][move.toCol] = EMPTY;
        //     }
        // }

// ...existing code...
        void makeMove(Move& move) {
            moveHistory.push_back(move); // Store the move in history
            Piece movedPiece = board[move.fromRow][move.fromCol];
            Piece pieceOnToSquare = board[move.toRow][move.toCol]; // Piece on destination before move

            // --- HASH UPDATES FOR OLD STATE (XORing out) ---
            // 1. Side to move (will be flipped by XORing again later)
            currentHash ^= zobristBlackToMove;

            // 2. Castling rights (before they change due to this move)
            int castlingKeyBefore = getCastlingRightsKey();
            currentHash ^= zobristCastlingRights[castlingKeyBefore];

            // 3. En passant square (if one exists)
            enPassantHistory.push_back(enPassantCol); // Store for undo
            if (enPassantCol != -1) {
                currentHash ^= zobristEnPassant[enPassantCol];
            }

            // 4. Piece being moved from its original square
            currentHash ^= zobristTable[movedPiece][move.fromRow][move.fromCol];

            // 5. Captured piece (if it's a standard capture)
            // For en-passant, pieceOnToSquare is EMPTY. Actual captured pawn handled later.
            if (pieceOnToSquare != EMPTY && !move.isEnPassant) {
                currentHash ^= zobristTable[pieceOnToSquare][move.toRow][move.toCol];
            }
            move.capturedPiece = pieceOnToSquare; // Store captured piece for later use
            // --- PERFORM BOARD STATE CHANGES ---
            // Store history for undo
            capturedPieces.push_back(move.capturedPiece); // Ensure move.capturedPiece is correctly set
            castlingRights.push_back(whiteKingMoved);
            castlingRights.push_back(blackKingMoved);
            castlingRights.push_back(whiteKingsideRookMoved);
            castlingRights.push_back(whiteQueensideRookMoved);
            castlingRights.push_back(blackKingsideRookMoved);
            castlingRights.push_back(blackQueensideRookMoved);

            // Move the piece
            board[move.toRow][move.toCol] = movedPiece;
            board[move.fromRow][move.fromCol] = EMPTY;

            Piece finalPieceOnToSquare = movedPiece; // This will be the piece XORed in at toSquare

            // Handle special move board updates & their specific hash components
            if (move.isEnPassant) {
                Piece actualCapturedPawn = (movedPiece == WPAWN) ? BPAWN : WPAWN;
                int capturedPawnActualRow = move.fromRow; // Same row as capturing pawn
                int capturedPawnActualCol = move.toCol;   // Same col as target square (where capturing pawn lands)
                
                board[capturedPawnActualRow][capturedPawnActualCol] = EMPTY; // Remove the captured pawn

                // Hash: XOR out the actual en-passant captured pawn
                currentHash ^= zobristTable[actualCapturedPawn][capturedPawnActualRow][capturedPawnActualCol];
            } else if (move.isCastling) {
                Piece rookPieceType;
                int rookOriginalCol, rookFinalCol;

                if (move.toCol > move.fromCol) { // Kingside castling
                    rookOriginalCol = 7; rookFinalCol = 5;
                } else { // Queenside castling
                    rookOriginalCol = 0; rookFinalCol = 3;
                }
                // Determine rook type based on whose turn it is (isWhiteTurn is BEFORE flip)
                rookPieceType = isWhiteTurn ? WROOK : BROOK;

                // Board update for rook
                board[move.fromRow][rookFinalCol] = rookPieceType;
                board[move.fromRow][rookOriginalCol] = EMPTY;

                // Hash: XOR out rook from original square, XOR in to new square
                currentHash ^= zobristTable[rookPieceType][move.fromRow][rookOriginalCol];
                currentHash ^= zobristTable[rookPieceType][move.fromRow][rookFinalCol];
            } else if (move.isPromotion) {
                // The pawn (movedPiece) is already on board[move.toRow][move.toCol]
                // We need to XOR out the pawn from toSquare before XORing in the promoted piece
                currentHash ^= zobristTable[movedPiece][move.toRow][move.toCol]; // XOR out pawn from toSquare

                board[move.toRow][move.toCol] = move.promotionPiece;
                finalPieceOnToSquare = move.promotionPiece; // This is what will be XORed in as the main piece on toSquare
            }

            // --- HASH UPDATES FOR NEW STATE (XORing in) ---
            // 6. Piece on the 'to' square (either the moved piece or promoted piece)
            currentHash ^= zobristTable[finalPieceOnToSquare][move.toRow][move.toCol];

            // 7. Update enPassantCol for next turn & its hash
            enPassantCol = -1; // Reset, then set if applicable
            if (movedPiece == WPAWN && move.fromRow == 1 && move.toRow == 3) {
                enPassantCol = move.fromCol;
            } else if (movedPiece == BPAWN && move.fromRow == 6 && move.toRow == 4) {
                enPassantCol = move.fromCol;
            }
            if (enPassantCol != -1) {
                currentHash ^= zobristEnPassant[enPassantCol];
            }

            // 8. Update castling flags
            if (movedPiece == WKING) whiteKingMoved = true;
            else if (movedPiece == BKING) blackKingMoved = true;
            else if (movedPiece == WROOK) {
                if (move.fromRow == 0 && move.fromCol == 0) whiteQueensideRookMoved = true;
                else if (move.fromRow == 0 && move.fromCol == 7) whiteKingsideRookMoved = true;
            }
            else if (movedPiece == BROOK) {
                if (move.fromRow == 7 && move.fromCol == 0) blackQueensideRookMoved = true;
                else if (move.fromRow == 7 && move.fromCol == 7) blackKingsideRookMoved = true;
            }
            // If a rook is captured, update its side's castling rights
            if (move.capturedPiece == WROOK) {
                if (move.toRow == 0 && move.toCol == 0) whiteQueensideRookMoved = true; // Rook at a1 captured
                else if (move.toRow == 0 && move.toCol == 7) whiteKingsideRookMoved = true; // Rook at h1 captured
            }
            else if (move.capturedPiece == BROOK) {
                if (move.toRow == 7 && move.toCol == 0) blackQueensideRookMoved = true; // Rook at a8 captured
                else if (move.toRow == 7 && move.toCol == 7) blackKingsideRookMoved = true; // Rook at h8 captured
            }
            
            int castlingKeyAfter = getCastlingRightsKey();
            currentHash ^= zobristCastlingRights[castlingKeyAfter];

            // 9. Side to move is already handled by the initial XOR and isWhiteTurn flip
            isWhiteTurn = !isWhiteTurn;
        }
// ...existing code...
        void undoMove(Move& move) {
            moveHistory.pop_back(); // Remove the last move from history
            // --- REVERT SIDE TO MOVE FIRST ---
            isWhiteTurn = !isWhiteTurn;
            currentHash ^= zobristBlackToMove; // XOR to flip side to move contribution

            // --- UNDO HASHING & BOARD STATE FOR NEW STATE OF MAKE MOVE (XORing out new state items) ---
            // 1. New Castling Rights (from end of makeMove)
            int castlingKeyAfterMakeMove = getCastlingRightsKey(); // This is the state *before* restoring flags
            currentHash ^= zobristCastlingRights[castlingKeyAfterMakeMove];

            // 2. New En Passant Square (from end of makeMove)
            // Note: enPassantCol is currently the one set by makeMove
            if (enPassantCol != -1) {
                currentHash ^= zobristEnPassant[enPassantCol];
            }

            // --- RESTORE BOARD FLAGS & PIECES (in reverse order of makeMove) ---
            // Restore castling flags from history
            blackQueensideRookMoved = castlingRights.back(); castlingRights.pop_back();
            blackKingsideRookMoved = castlingRights.back(); castlingRights.pop_back();
            whiteQueensideRookMoved = castlingRights.back(); castlingRights.pop_back();
            whiteKingsideRookMoved = castlingRights.back(); castlingRights.pop_back();
            blackKingMoved = castlingRights.back(); castlingRights.pop_back();
            whiteKingMoved = castlingRights.back(); castlingRights.pop_back();

            // Restore enPassantCol from history
            enPassantCol = enPassantHistory.back(); enPassantHistory.pop_back();


            Piece pieceThatMoved = board[move.toRow][move.toCol]; // This is the piece after promotion, if any

            // 3. Piece on 'to' square (XOR out what was added at end of makeMove)
            currentHash ^= zobristTable[pieceThatMoved][move.toRow][move.toCol];


            // --- UNDO BOARD CHANGES & SPECIAL MOVES HASHING ---
            move.capturedPiece = capturedPieces.back(); // Get the last captured piece from history
            // Revert general move
            board[move.fromRow][move.fromCol] = (move.isPromotion ? (isWhiteTurn ? WPAWN : BPAWN) : pieceThatMoved);
            if(!move.isEnPassant) board[move.toRow][move.toCol] = move.capturedPiece; // Restore captured piece or EMPTY

            if (move.isPromotion) {
                // pieceThatMoved was the promoted piece. We XORed it out.
                // Now, the pawn is back on fromRow.
                // The original pawn (before promotion) needs to be XORed back in at fromRow.
                // This is handled by step 6.
            } else if (move.isEnPassant) {
                // pieceThatMoved was the capturing pawn. It's now back on fromRow.
                // The captured piece (move.capturedPiece) was EMPTY on toRow.
                Piece actualCapturedPawn = (isWhiteTurn ? BPAWN : WPAWN); // Pawn that was captured
                int capturedPawnActualRow = move.fromRow; // Same row as where capturing pawn moved from
                int capturedPawnActualCol = move.toCol;   // Same col as where capturing pawn moved to
                
                board[capturedPawnActualRow][capturedPawnActualCol] = actualCapturedPawn; // Restore captured pawn
                board[move.toRow][move.toCol] = EMPTY; // Clear the toSquare

                // Hash: XOR in the restored en-passant captured pawn
                currentHash ^= zobristTable[actualCapturedPawn][capturedPawnActualRow][capturedPawnActualCol];
            } else if (move.isCastling) {
                // pieceThatMoved was the King. It's now back on fromRow.
                // The rook also needs to be moved back and its hash updated.
                Piece rookPieceType;
                int rookOriginalCol, rookFinalCol;

                if (move.toCol > move.fromCol) { // Kingside castling (King moved to col 6, Rook to col 5 from 7)
                    rookOriginalCol = 7; rookFinalCol = 5;
                } else { // Queenside castling (King moved to col 2, Rook to col 3 from 0)
                    rookOriginalCol = 0; rookFinalCol = 3;
                }
                rookPieceType = isWhiteTurn ? WROOK : BROOK; // Rook of the player whose turn it was

                // Board update: Move rook back
                board[move.fromRow][rookOriginalCol] = rookPieceType;
                board[move.fromRow][rookFinalCol] = EMPTY;

                // Hash: XOR out rook from its castled square, XOR in to original square
                currentHash ^= zobristTable[rookPieceType][move.fromRow][rookFinalCol];
                currentHash ^= zobristTable[rookPieceType][move.fromRow][rookOriginalCol];
            }
            
            // --- RESTORE HASHING FOR OLD STATE OF MAKE MOVE (XORing in old state items) ---
            // 4. Piece being moved back to its original square
            currentHash ^= zobristTable[board[move.fromRow][move.fromCol]][move.fromRow][move.fromCol];

            // 5. Captured piece (if it was a standard capture)
            if (move.capturedPiece != EMPTY && !move.isEnPassant) {
                currentHash ^= zobristTable[move.capturedPiece][move.toRow][move.toCol];
            }
            
            // 6. Old En Passant Square (restored enPassantCol)
            if (enPassantCol != -1) {
                currentHash ^= zobristEnPassant[enPassantCol];
            }

            // 7. Old Castling Rights (after flags are restored)
            int castlingKeyBeforeMakeMove = getCastlingRightsKey();
            currentHash ^= zobristCastlingRights[castlingKeyBeforeMakeMove];

            // 8. Side to move is already handled.

            // Restore captured piece from history (for other game logic if needed)
            capturedPieces.pop_back();
        }
// ...existing code...
        bool isInCheck() {
            int kingRow = -1, kingCol = -1;
            Piece kingPiece = isWhiteTurn ? WKING : BKING;

            // Find the king
            for (int i = 0; i < 8 && kingRow == -1; ++i) {
                for (int j = 0; j < 8; ++j) {
                    if (board[i][j] == kingPiece) {
                        kingRow = i;
                        kingCol = j;
                        break;
                    }
                }
            }

            bool forWhite = isWhiteTurn;
            return isUnderAttack(kingRow, kingCol, forWhite);
        }
        bool isLegalPieceMove(Piece piece, Move& move) {
            switch (piece) {
                case WPAWN: case BPAWN:
                    return isValidPawnMove(move);
                case WROOK: case BROOK:
                    return isValidRookMove(move);
                case WKNIGHT: case BKNIGHT:
                    return isValidKnightMove(move);
                case WBISHOP: case BBISHOP:
                    return isValidBishopMove(move);
                case WQUEEN: case BQUEEN:
                    return isValidQueenMove(move);
                case WKING: case BKING:
                    return isValidKingMove(move);
                default:
                    return false;
            }
        }


        bool isCheckmate() {
            if (!isInCheck()) return false;
            std::vector<Move> validMoves = generateValidMoves();
            return validMoves.empty();
        }

        bool isStalemate() {
            if (isInCheck()) return false;

            std::vector<Move> validMoves = generateValidMoves();
            return validMoves.empty();
        }

        int evaluateBoard() {
            bool lateGame = isLateGame();
            const int pawnValue = 100, knightValue = 300, bishopValue = 300, rookValue = 500, queenValue = 900, kingValue = 10000;

            int whiteMaterial = 0, blackMaterial = 0;
            int whitePositional = 0, blackPositional = 0;

            // Evaluate white pieces
            for (int i = 0; i < 8; ++i) {
                for (int j = 0; j < 8; ++j) {
                    Piece piece = board[i][j];
                    switch (piece) {
                        case WPAWN:
                            whiteMaterial += pawnValue;
                            whitePositional += pawnTable[i][j];
                            break;
                        case WKNIGHT:
                            whiteMaterial += knightValue;
                            whitePositional += knightTable[i][j];
                            break;
                        case WBISHOP:
                            whiteMaterial += bishopValue;
                            whitePositional += bishopTable[i][j];
                            break;
                        case WROOK:
                            whiteMaterial += rookValue;
                            whitePositional += rookTable[i][j];
                            break;
                        case WQUEEN:
                            whiteMaterial += queenValue;
                            whitePositional += queenTable[i][j];
                            break;
                        case WKING:
                            whiteMaterial += kingValue;
                            if (lateGame) {
                                whitePositional += whiteKingTableLate[i][j];  // Late-game table
                            } else {
                                whitePositional += whiteKingTableEarly[i][j];  // Early-game table
                            }
                            break;

                            // Black pieces (inverted table access)
                        case BPAWN:
                            blackMaterial += pawnValue;
                            blackPositional += pawnTable[7 - i][j];  // Flip rows for black
                            break;
                        case BKNIGHT:
                            blackMaterial += knightValue;
                            blackPositional += knightTable[7 - i][j];
                            break;
                        case BBISHOP:
                            blackMaterial += bishopValue;
                            blackPositional += bishopTable[7 - i][j];
                            break;
                        case BROOK:
                            blackMaterial += rookValue;
                            blackPositional += rookTable[7 - i][j];
                            break;
                        case BQUEEN:
                            blackMaterial += queenValue;
                            blackPositional += queenTable[7 - i][j];
                            break;
                        case BKING:
                            blackMaterial += kingValue;
                            if (lateGame) {
                                blackPositional += blackKingTableLate[i][j];  // Late-game table
                            } else {
                                blackPositional += blackKingTableEarly[i][j];  // Early-game table
                            }
                            break;
                        default:
                            break;
                    }
                }
            }

            // Material and positional score difference
            return (whiteMaterial + whitePositional) - (blackMaterial + blackPositional);
        }
        int getPieceValue(Piece piece) {
            switch (piece) {
                case WPAWN: case BPAWN: return 100;
                case WKNIGHT: case BKNIGHT: return 300;
                case WBISHOP: case BBISHOP: return 300;
                case WROOK: case BROOK: return 500;
                case WQUEEN: case BQUEEN: return 900;
                case WKING: case BKING: return 10000;
                default: return 0;
            }
        }
        bool wouldGiveCheck(Move& move) {
            // Make the move
            makeMove(move);

            // Check if opponent is in check
            bool givesCheck = isInCheck();

            // Undo the move
            undoMove(move);

            return givesCheck;
        }

        int scoreMove(Move& move) {
            int score = 0;
            move.capturedPiece = move.isEnPassant ? (board[move.fromRow][move.toCol] == BPAWN ? BPAWN : WPAWN) : board[move.toRow][move.toCol];
            // Higher score for captures (MVV-LVA: Most Valuable Victim - Least Valuable Attacker)
            if (move.capturedPiece != EMPTY) {
                int victimValue = getPieceValue(move.capturedPiece);
                int attackerValue = getPieceValue(board[move.fromRow][move.fromCol]);
                score += 10 * victimValue - attackerValue;
            }

            // Bonus for promotions
            if (move.isPromotion) {
                score += 800;  // Queen promotion is highly valuable
            }

            // Bonus for checks
            if (wouldGiveCheck(move)) {
                score += 50;
            }

            // Penalize moving the same piece again (could add history heuristic)
            // Additional heuristics can be added here

            return score;
        }


        std::vector<Move> generateValidMoves() {
            std::vector<Move> validMoves;
            for (int fromRow = 0; fromRow < 8; ++fromRow) {
                for (int fromCol = 0; fromCol < 8; ++fromCol) {
                    if (board[fromRow][fromCol] != EMPTY && isWhiteTurn == (board[fromRow][fromCol] <= WKING)) {
                        for (int toRow = 0; toRow < 8; ++toRow) {
                            for (int toCol = 0; toCol < 8; ++toCol) {
                                    Move move = {fromRow, fromCol, toRow, toCol};
                                if (isValidMove(move)) {
                                    // Assign heuristic score
                                    move.moveScore = scoreMove(move);
                                    validMoves.push_back(move);
                                }
                            }
                        }
                    }
                }
            }
            return validMoves;
        }

        void sendInfo(int depth, int score, int elapsedTime) {
            std::cout << "info depth " << depth
                      << " score cp " << score
                      << " time " << elapsedTime
                      << " nodes " << nodesSearched << std::endl;
        }

        std::pair<int, Move> negamax(int depth, int alpha, int beta, bool isMaximizing) {
            // Lookup in the transposition table
            int alphaOrig = alpha;

            nodesSearched++;
            // Check for time to send an info message
            static auto lastInfoTime = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            int elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(now - searchStartTime).count();
            int infoInterval = 1000; // Send info every 1 second

            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastInfoTime).count() >= infoInterval) {
                // Send info message
                sendInfo(depth, alpha, elapsedTime);

                lastInfoTime = now;
            }
            // if (transpositionTable.find(currentHash) != transpositionTable.end()) {
            //     TTEntry& entry = transpositionTable[currentHash];
            //     if (entry.depth >= depth) {
            //         if (entry.flag == TTEntry::EXACT) {
            //             return {entry.score, entry.bestMove};
            //         } else if (entry.flag == TTEntry::LOWERBOUND) {
            //             alpha = std::max(alpha, entry.score);
            //         } else if (entry.flag == TTEntry::UPPERBOUND) {
            //             beta = std::min(beta, entry.score);
            //         }
            //         if (alpha >= beta) {
            //             return {entry.score, entry.bestMove};
            //         }
            //     }
            // }
            if(isCheckmate()) return {-20000, Move()};
            if(isStalemate()) return {0, Move()};
            if (depth == 0) {
                return {(isMaximizing ? 1 : -1) * evaluateBoard(), Move()};  // Return the score and an empty move
            }

            int bestScore = -100000;
            Move bestMove;
            bestMove.fromRow = -1;

            std::vector<Move> validMoves = generateValidMoves();  // Implement this function
            // Sort moves by moveScore in descending order
            std::sort(validMoves.begin(), validMoves.end(), [](const Move& a, const Move& b) {
                return a.moveScore > b.moveScore;
            });

            for (Move& move : validMoves) {
                // Apply the move
                makeMove(move);

                // Perform negamax search on the resulting board state
                int score = -negamax(depth - 1, -beta, -alpha, !isMaximizing).first;

                // Undo the move
                undoMove(move);

                if (score > bestScore || bestMove.fromRow == -1) {
                    bestScore = score;
                    bestMove = move;
                }

                alpha = std::max(alpha, score);

                if (alpha >= beta) {
                    break;  // Alpha-beta pruning
                }
            }
            // Store in the transposition table
            TTEntry entry;
            entry.hash = currentHash;
            entry.depth = depth;
            entry.score = bestScore;
            entry.bestMove = bestMove;
            if (bestScore <= alphaOrig) {
                entry.flag = TTEntry::UPPERBOUND;
            } else if (bestScore >= beta) {
                entry.flag = TTEntry::LOWERBOUND;
            } else {
                entry.flag = TTEntry::EXACT;
            }
            transpositionTable[currentHash] = entry;

            return {bestScore, bestMove};
        }


        void aiMove() {
            int depth = 7;  // Depth of the search, increase for stronger AI

            // Replace structured binding with explicit pair
            std::pair<int, Move> result = negamax(depth, -10000, 10000, !userIsWhite);
            int bestScore = result.first;
            Move bestMove = result.second;

            // Apply the best move
            makeMove(bestMove);
            std::cout << "AI moved from " << (char)('a' + bestMove.fromCol) << (bestMove.fromRow + 1)
                      << " to " << (char)('a' + bestMove.toCol) << (bestMove.toRow + 1) << std::endl;

            if (bestMove.isPromotion) {
                std::cout << "AI promoted a pawn to a " << (isWhiteTurn ? "White Queen!" : "Black Queen!") << std::endl;
            }
        }



        void play() {
            pickSide();
            std::string moveStr;
            while (true) {
                printBoard();

                if (isCheckmate()) {
                    std::cout << (isWhiteTurn ? "Black" : "White") << " wins by checkmate!" << std::endl;
                    break;
                }
                if (isStalemate()) {
                    std::cout << "The game is a draw by stalemate." << std::endl;
                    break;
                }

                if (isHumanTurn()) {
                    std::cout << "Your move (e.g., e2e4 or 0-0 for castling): ";
                    std::cin >> moveStr;

                    if (moveStr == "quit") break;

                    Move move;
                    if (moveStr == "0-0" || moveStr == "O-O") {
                        move = {0, 4, 0, 6};  // Kingside castling
                    } else if (moveStr == "0-0-0" || moveStr == "O-O-O") {
                        move = {0, 4, 0, 2};  // Queenside castling
                    } else if (moveStr.length() == 4) {
                        move.fromCol = moveStr[0] - 'a';
                        move.fromRow = moveStr[1] - '1';
                        move.toCol = moveStr[2] - 'a';
                        move.toRow = moveStr[3] - '1';
                    } else {
                        std::cout << "Invalid input. Please use format 'e2e4' or '0-0' for castling." << std::endl;
                        continue;
                    }

                    if (isValidMove(move)) {
                        if (board[move.fromRow][move.fromCol] == WPAWN && move.toRow == 7) {
                            char promotionPiece;
                            std::cout << "Promote pawn to (Q/R/B/N): ";
                            std::cin >> promotionPiece;
                            switch (toupper(promotionPiece)) {
                                case 'Q': move.promotionPiece = WQUEEN; break;
                                case 'R': move.promotionPiece = WROOK; break;
                                case 'B': move.promotionPiece = WBISHOP; break;
                                case 'N': move.promotionPiece = WKNIGHT; break;
                                default: std::cout << "Invalid promotion piece. Promoting to Queen." << std::endl;
                                    move.promotionPiece = WQUEEN;
                            }
                        }
                        makeMove(move);
                    } else {
                        std::cout << "Invalid move. Try again." << std::endl;
                        continue;
                    }
                } else {
                    aiMove();
                }
            }
        }
    };

    class UCIEngine {
    private:
        ChessEngine engine;

    public:
        void setPositionFromFEN(const std::string& fen) {
            // Reset the board
            engine.board = std::vector<std::vector<Piece>>(8, std::vector<Piece>(8, EMPTY));

            std::istringstream iss(fen);
            std::string boardPart, activeColor, castling, enPassant, halfmoveClock, fullmoveNumber;

            // Split the FEN string into its components
            iss >> boardPart >> activeColor >> castling >> enPassant >> halfmoveClock >> fullmoveNumber;

            // Parse the board
            int row = 7;
            int col = 0;
            for (char c : boardPart) {
                if (c == '/') {
                    row--;
                    col = 0;
                } else if (isdigit(c)) {
                    col += c - '0';
                } else {
                    Piece piece = EMPTY;
                    switch (c) {
                        case 'P': piece = WPAWN; break;
                        case 'N': piece = WKNIGHT; break;
                        case 'B': piece = WBISHOP; break;
                        case 'R': piece = WROOK; break;
                        case 'Q': piece = WQUEEN; break;
                        case 'K': piece = WKING; break;
                        case 'p': piece = BPAWN; break;
                        case 'n': piece = BKNIGHT; break;
                        case 'b': piece = BBISHOP; break;
                        case 'r': piece = BROOK; break;
                        case 'q': piece = BQUEEN; break;
                        case 'k': piece = BKING; break;
                        default: break;
                    }
                    engine.board[row][col] = piece;
                    col++;
                }
            }

            // Parse active color
            engine.isWhiteTurn = (activeColor == "w");

            // Reset castling rights
            engine.whiteKingMoved = engine.blackKingMoved = false;
            engine.whiteKingsideRookMoved = engine.whiteQueensideRookMoved = false;
            engine.blackKingsideRookMoved = engine.blackQueensideRookMoved = false;

            // Parse castling availability
            if (castling.find('K') != std::string::npos) {
                engine.whiteKingMoved = true;
                engine.whiteKingsideRookMoved = true;
            }
            if (castling.find('Q') != std::string::npos) {
                engine.whiteKingMoved = true;
                engine.whiteQueensideRookMoved = true;
            }
            if (castling.find('k') != std::string::npos) {
                engine.blackKingMoved = true;
                engine.blackKingsideRookMoved = true;
            }
            if (castling.find('q') != std::string::npos) {
                engine.blackKingMoved = false;
                engine.blackQueensideRookMoved = false;
            }

            // Parse en passant target square
            if (enPassant != "-") {
                engine.enPassantCol = enPassant[0] - 'a';
                // You might also want to store the en passant row if needed
            } else {
                engine.enPassantCol = -1;
            }

            // Reinitialize Zobrist hashing
            initializeZobrist();

            // Compute the initial hash
            currentHash = 0;
            for (int row = 0; row < 8; ++row) {
                for (int col = 0; col < 8; ++col) {
                    Piece piece = engine.board[row][col];
                    if (piece != EMPTY) {
                        currentHash ^= zobristTable[piece][row][col];
                    }
                }
            }

            // Include side to move
            if (!engine.isWhiteTurn) {
                currentHash ^= zobristBlackToMove;
            }

            // Include castling rights
            currentHash ^= zobristCastlingRights[engine.getCastlingRightsKey()];

            // Include en passant
            if (engine.enPassantCol != -1) {
                currentHash ^= zobristEnPassant[engine.enPassantCol];
            }
        }

        Move convertUCIStringToMove(const std::string& moveStr) {
            Move move;
            move.fromCol = moveStr[0] - 'a';
            move.fromRow = moveStr[1] - '1';
            move.toCol = moveStr[2] - 'a';
            move.toRow = moveStr[3] - '1';

            // Handle promotion
            if (moveStr.length() == 5) {
                char promoChar = moveStr[4];
                move.isPromotion = true;
                switch (promoChar) {
                    case 'q':
                        move.promotionPiece = engine.isWhiteTurn ? WQUEEN : BQUEEN;
                        break;
                    case 'r':
                        move.promotionPiece = engine.isWhiteTurn ? WROOK : BROOK;
                        break;
                    case 'b':
                        move.promotionPiece = engine.isWhiteTurn ? WBISHOP : BBISHOP;
                        break;
                    case 'n':
                        move.promotionPiece = engine.isWhiteTurn ? WKNIGHT : BKNIGHT;
                        break;
                    default:
                        move.isPromotion = false; // Invalid promotion piece
                        break;
                }
            }

            // Determine if the move is castling
            Piece piece = engine.board[move.fromRow][move.fromCol];
            if ((piece == WKING || piece == BKING) && std::abs(move.toCol - move.fromCol) == 2) {
                move.isCastling = true;
            }

            // Determine if the move is en passant
            if ((piece == WPAWN || piece == BPAWN) && move.toCol != move.fromCol && engine.board[move.toRow][move.toCol] == EMPTY) {
                move.isEnPassant = true;
            }

            return move;
        }

        void uciLoop() {
            std::string line;
            while (std::getline(std::cin, line)) {
                std::istringstream iss(line);
                std::string token;
                iss >> token;

                if (token == "uci") {
                    std::cout << "id name YourEngineName" << std::endl;
                    std::cout << "id author YourName" << std::endl;
                    // Add your engine options here
                    std::cout << "uciok" << std::endl;
                }
                else if (token == "isready") {
                    std::cout << "readyok" << std::endl;
                }
                else if (token == "ucinewgame") {
                    engine = ChessEngine(); // Reset the engine for a new game
                }
                else if (token == "position") {
                    handlePosition(iss);
                }
                else if (token == "go") {
                    handleGo(iss);
                }
                else if (token == "quit") {
                    break;
                }
            }
        }
        void handlePosition(std::istringstream& iss) {
            std::string token;
            iss >> token;

            if (token == "startpos") {
                engine = ChessEngine(); // Reset to starting position
                if (!(iss >> token)) {
                    return; // No further tokens
                }
            }
            else if (token == "fen") {
                // Read the FEN string (6 fields)
                std::string fen, temp;
                int fieldsRead = 0;
                while (fieldsRead < 6 && iss >> temp) {
                    if (temp == "moves") {
                        break;
                    }
                    fen += temp + " ";
                    fieldsRead++;
                }
                //setPositionFromFEN(fen);
                engine = ChessEngine();
                token = temp; // Check if "moves" follows
            }

            // Apply moves if any
            if (token == "moves") {
                std::string moveStr;
                while (iss >> moveStr) {
                    Move move = convertUCIStringToMove(moveStr);
                    if (engine.isValidMove(move)) {
                        engine.makeMove(move);
                    } else {
                        std::cerr << "Invalid move: " << moveStr << std::endl;
                    }
                }
            }
        }

        void handleGo(std::istringstream& iss) {
            // Parse time control information if needed
            int depth = 5;

            // Replace structured binding with explicit pair
            std::pair<int, Move> result = engine.negamax(depth, -10000, 10000, engine.isWhiteTurn);
            int score = result.first;
            Move bestMove = result.second;

            // Convert your Move to a string in UCI format
            std::string moveStr =
                    std::string(1, 'a' + bestMove.fromCol) +
                    std::string(1, '1' + bestMove.fromRow) +
                    std::string(1, 'a' + bestMove.toCol) +
                    std::string(1, '1' + bestMove.toRow);

            // If it's a promotion, append the promotion piece
            if (bestMove.isPromotion) {
                switch (bestMove.promotionPiece) {
                    case WQUEEN: case BQUEEN: moveStr += 'q'; break;
                    case WROOK: case BROOK: moveStr += 'r'; break;
                    case WBISHOP: case BBISHOP: moveStr += 'b'; break;
                    case WKNIGHT: case BKNIGHT: moveStr += 'n'; break;
                    default: break;
                }
            }

            std::cout << "bestmove " << moveStr << " "<< score << std::endl;

            // Apply the move to your internal board
            engine.makeMove(bestMove);
        }

    };


    int main() {
        UCIEngine uciEngine;
        uciEngine.uciLoop();
        return 0;
    }
