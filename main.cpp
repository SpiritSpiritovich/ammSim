#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <stdexcept>
//#include <cmath>

// Holds swap outputs required by the task.
struct SwapResult {
    double amountOut{};        // how many tokens user receives
    double newReserveA{};      // reserveA after swap
    double newReserveB{};      // reserveB after swap
    double effectivePrice{};   // amountOut / amountIn (units depend on direction)
    double slippagePercent{};
};

// Simple validation helper
// msg error message if false
static void require(bool cond, const std::string& msg) {
    if (!cond) throw std::runtime_error(msg);
}

// Uniswap v2-style formula:
// amountInWithFee = amountIn * (1 - fee)
// amountOut = (amountInWithFee * reserveOut) / (reserveIn + amountInWithFee)
static double getAmountOut(double amountIn, double reserveIn, double reserveOut, double fee) {
    require(amountIn > 0.0, "amountIn must be > 0");
    require(reserveIn > 0.0 && reserveOut > 0.0, "reserves must be > 0");
    require(fee >= 0.0 && fee < 1.0, "fee must be in [0, 1)");

    // Apply fee to input amount (0.3% fee => keep 99.7% for pricing)
    const double amountInWithFee = amountIn * (1.0 - fee);

    // Constant-product swap output (same math as Uniswap v2 library)
    return (amountInWithFee * reserveOut) / (reserveIn + amountInWithFee);
}

// direction: "A2B" or "B2A"
// spot price before trade:
//  - A2B: P0 = reserveB / reserveA (B per A)
//  - B2A: P0 = reserveA / reserveB (A per B)
// effective price:
//  - Peff = amountOut / amountIn
// slippage% = (P0 - Peff) / P0 * 100
static SwapResult simulateSwap(double reserveA, double reserveB, double fee,
                               const std::string& directionRaw, double amountIn) {
    require(reserveA > 0.0 && reserveB > 0.0, "reserveA and reserveB must be > 0");

    // Normalize direction to uppercase so "a2b" works too.
    std::string direction = directionRaw;
    for (auto& c : direction) c = (char)std::toupper((unsigned char)c);

    require(direction == "A2B" || direction == "B2A", "direction must be A2B or B2A");

    SwapResult r{};

    if (direction == "A2B") {
        // Spot price (before trade): how many B for 1 A
        const double P0 = reserveB / reserveA;

        const double out = getAmountOut(amountIn, reserveA, reserveB, fee);
        require(out < reserveB, "amountOut would drain the pool (invalid trade)");

        // Update pool reserves after swap
        r.amountOut = out;
        r.newReserveA = reserveA + amountIn;
        r.newReserveB = reserveB - out;

        // Effective price for this trade
        r.effectivePrice = out / amountIn;      // B per A

        // Slippage relative to spot price
        r.slippagePercent = (P0 - r.effectivePrice) / P0 * 100.0;
    } else { // B2A
        const double P0 = reserveA / reserveB; // A per B

        const double out = getAmountOut(amountIn, reserveB, reserveA, fee);
        require(out < reserveA, "amountOut would drain the pool (invalid trade)");

        r.amountOut = out;
        r.newReserveA = reserveA - out;
        r.newReserveB = reserveB + amountIn;

        r.effectivePrice = out / amountIn;      // A per B
        r.slippagePercent = (P0 - r.effectivePrice) / P0 * 100.0;
    }

    return r;
}

// Scenario for demo (name + direction + amountIn)
struct Scenario {
    std::string name;
    std::string direction;
    double amountIn;
};

static void printHeader() {
    std::cout
            << std::left
            << std::setw(10) << "Scenario"
            << std::setw(6)  << "Dir"
            << std::right
            << std::setw(12) << "amountIn"
            << std::setw(14) << "amountOut"
            << std::setw(14) << "newResA"
            << std::setw(14) << "newResB"
            << std::setw(16) << "effPrice"
            << std::setw(14) << "slip(%)"
            << "\n";
    std::cout << std::string(100, '-') << "\n";
}

static void printRow(const Scenario& s, const SwapResult& r) {
    // setw(N) affects ONLY the next value printed.
    // fixed + setprecision(K) prints K digits after decimal point.
    std::cout
            << std::left
            << std::setw(10) << s.name
            << std::setw(6)  << s.direction
            << std::right
            << std::setw(12) << std::fixed << std::setprecision(6) << s.amountIn
            << std::setw(14) << std::fixed << std::setprecision(6) << r.amountOut
            << std::setw(14) << std::fixed << std::setprecision(6) << r.newReserveA
            << std::setw(14) << std::fixed << std::setprecision(6) << r.newReserveB
            << std::setw(16) << std::fixed << std::setprecision(8) << r.effectivePrice
            << std::setw(14) << std::fixed << std::setprecision(6) << r.slippagePercent
            << "\n";
}

static void printUsage(const char* prog) {
    std::cout <<
              "Usage:\n"
              "  " << prog << " --reserveA <num> --reserveB <num> --fee <num> --direction A2B|B2A --amountIn <num>\n"
                              "  " << prog << " --demo\n\n"
                                              "Note:\n"
                                              "  If you run without arguments, program runs demo mode by default.\n\n"
                                              "Examples:\n"
                                              "  " << prog << " --demo\n"
                                                              "  " << prog << " --reserveA 10000 --reserveB 10000 --fee 0.003 --direction A2B --amountIn 100\n";
}

static bool hasFlag(const std::vector<std::string>& args, const std::string& flag) {
    for (const auto& a : args) if (a == flag) return true;
    return false;
}

static std::string getArg(const std::vector<std::string>& args, const std::string& key) {
    for (size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == key) return args[i + 1];
    }
    return "";
}

static double toDouble(const std::string& s, const std::string& name) {
    require(!s.empty(), "Missing value for " + name);

    size_t idx = 0;
    double v = std::stod(s, &idx);

    // Ensure the whole string is a number (no trailing junk)
    require(idx == s.size(), "Invalid number for " + name + ": " + s);
    return v;
}

// Runs the required 3 scenarios and prints a table + conclusions.
// (Used for --demo and also default run with no args.)
static int runDemo() {
    // Default pool (can represent any pair, BNB/USDT and so on)
    const double reserveA = 10000.0;
    const double reserveB = 10000.0;
    const double fee = 0.003;          // 0.3%
    const std::string direction = "A2B";

    std::vector<Scenario> scenarios = {
            {"small",  direction, reserveA * 0.01},  // 1% of reserveA
            {"medium", direction, reserveA * 0.10},  // 10%
            {"large",  direction, reserveA * 0.40},  // 40%
    };

    std::cout << "Demo: reserveA=" << reserveA << ", reserveB=" << reserveB
              << ", fee=" << fee << ", direction=" << direction << "\n\n";

    printHeader();
    for (const auto& s : scenarios) {
        auto r = simulateSwap(reserveA, reserveB, fee, s.direction, s.amountIn);
        printRow(s, r);
    }

    std::cout << "\nConclusions:\n"
              << "- Slippage grows non-linearly with trade size (big trades move reserves a lot).\n"
              << "- Effective price is always worse than spot because of fee + price impact.\n"
              << "- Larger pools (more liquidity) mean smaller slippage for the same amountIn.\n";

    return 0;
}

int main(int argc, char** argv) {
    try {
        std::vector<std::string> args;
        args.reserve((size_t)argc);
        for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);

        // If user presses Run without arguments -> run demo automatically.
        if (args.empty()) {
            return runDemo();
        }

        if (hasFlag(args, "--help") || hasFlag(args, "-h")) {
            printUsage(argv[0]);
            return 0;
        }

        if (hasFlag(args, "--demo")) {
            return runDemo();
        }

        // Single-run mode (custom swap from arguments)
        const double reserveA = toDouble(getArg(args, "--reserveA"), "--reserveA");
        const double reserveB = toDouble(getArg(args, "--reserveB"), "--reserveB");
        const double fee      = toDouble(getArg(args, "--fee"),      "--fee");
        const std::string dir = getArg(args, "--direction");
        const double amountIn = toDouble(getArg(args, "--amountIn"), "--amountIn");

        auto r = simulateSwap(reserveA, reserveB, fee, dir, amountIn);

        std::cout << std::fixed << std::setprecision(10);
        std::cout << "amountOut       = " << r.amountOut << "\n";
        std::cout << "new reserveA    = " << r.newReserveA << "\n";
        std::cout << "new reserveB    = " << r.newReserveB << "\n";
        std::cout << "effective price = " << r.effectivePrice << "\n";
        std::cout << "slippage (%)    = " << std::setprecision(6) << r.slippagePercent << "\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        std::cerr << "Run with --help for usage.\n";
        return 1;
    }
}