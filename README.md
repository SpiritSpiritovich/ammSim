# Constant-Product AMM Simulator (Uniswap v2 style)

## Overview

This project implements a simple simulator of a constant-product Automated Market Maker (AMM), similar to Uniswap v2.

Each liquidity pool contains two tokens:

```
reserveA = x
reserveB = y
```

The invariant:

```
x * y = k
```

After every swap, the product of reserves remains constant (ignoring fee growth).

---

## Formula Used

###  Fee-adjusted input

```
amountInWithFee = amountIn * (1 - fee)
```

###  Output amount

```
amountOut =
(amountInWithFee * reserveOut)
/
(reserveIn + amountInWithFee)
```

###  Spot price (before trade)

For A → B:

```
P0 = reserveB / reserveA
```

###  Effective price (actual trade price)

```
Peff = amountOut / amountIn
```

###  Slippage

```
slippage % = (P0 - Peff) / P0 * 100
```

---

## Scenarios (Demo Mode)

Initial pool:

```
reserveA = 10000
reserveB = 10000
fee = 0.003 (0.3%)
```

| Scenario     | amountIn | amountOut | Slippage |
| ------------ | -------- | --------- | -------- |
| Small (1%)   | 100      | ~98.72    | ~1.28%   |
| Medium (10%) | 1000     | ~906.61   | ~9.34%   |
| Large (40%)  | 4000     | ~2851.02  | ~28.72%  |

---

## Conclusions

* Slippage increases non-linearly as trade size grows.
* Large trades significantly move the price along the x*y=k curve.
* Most of the loss in large trades comes from price impact, not just fee.
* Higher liquidity reduces slippage.

---

## How to Run

### Demo mode (3 scenarios)

```
crypt.exe --demo or just Run it.
```

### Custom swap

```
crypt.exe --reserveA 10000 --reserveB 10000 --fee 0.003 --direction A2B --amountIn 100
```

### Hardcode inside a "static int runDemo()"
```

    const double reserveA = 10000.0;
    const double reserveB = 10000.0;
    const double fee = 0.003;          // 0.3%
    const std::string direction = "A2B";
