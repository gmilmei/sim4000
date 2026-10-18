#!/usr/bin/env python3
"""Independent FP checks using exact rational and signed-integer arithmetic.

Reference: RC4000 Reference Manual, June 1969, sections 5.1-5.6 (pp. 22-26).
This oracle does not reproduce the C limb product, restoring division, or
guard-register normalization algorithms, and never uses host floating point.
FA/FS model historical alignment using unbounded signed integers, not the C
register-bit/overflow implementation. A separate 38-bit model exposes manual
differences. Non-normalized arithmetic checks retain intermediate truncation;
large division quotients are scaled into range before guard quantization.
"""

from fractions import Fraction
import itertools
import pathlib
import random
import subprocess
import sys


SCALE = 1 << 35
HALF = 1 << 34
INTEGER_MIN = -(1 << 23)
INTEGER_MAX = (1 << 23) - 1


def from_integer(integer, scale):
    fraction, exponent = normalized(Fraction(integer), scale)
    return fraction, exponent, not -2048 <= exponent <= 2047


def to_integer(value, scale):
    fraction, exponent = value
    power = exponent + scale
    # Exact bounds avoid creating enormous powers for 24-bit address scales.
    # A nonzero 36-bit fraction times 2**power exceeds the integer range
    # for power > 60; for power < -60 even the largest fraction rounds to zero.
    if not fraction or power < -60:
        return 0, False
    if power > 60:
        return 99, True
    exact = Fraction(fraction, SCALE)
    exact = exact * (1 << power) if power >= 0 else exact / (1 << -power)
    rounded = exact + Fraction(1, 2)
    integer = rounded.numerator // rounded.denominator
    overflow = not INTEGER_MIN <= integer <= INTEGER_MAX
    return (99 if overflow else integer), overflow


def normalized(value, exponent):
    """Choose the signed normalized interval, then round ties toward +infinity."""
    if not value:
        return 0, -2048
    if value > 0:
        while value < Fraction(1, 2):
            value *= 2
            exponent -= 1
        while value >= 1:
            value /= 2
            exponent += 1
    else:
        # Two's complement makes the negative interval [-1, -1/2), not
        # the mirror of the positive interval [1/2, 1).
        while value >= Fraction(-1, 2):
            value *= 2
            exponent -= 1
        while value < -1:
            value /= 2
            exponent += 1
    rounded = value * SCALE + Fraction(1, 2)
    fraction = rounded.numerator // rounded.denominator
    if fraction == SCALE:
        fraction //= 2
        exponent += 1
    elif fraction == -HALF:
        fraction *= 2
        exponent -= 1
    return fraction, exponent


def arithmetic(opcode, left, right):
    if opcode == 52 and right[0] == 0:
        return *left, True, True
    if opcode == 50:
        value = Fraction(left[0] * right[0], SCALE * SCALE)
        exponent = left[1] + right[1]
    else:
        value = Fraction(left[0], right[0])
        exponent = left[1] - right[1]
    fraction, exponent = normalized(value, exponent)
    return fraction, exponent, not -2048 <= exponent <= 2047, False


def quantized_arithmetic(opcode, left, right):
    """Retain the 38-bit fraction lattice before cancellation normalization.

    This is a compatibility model, not ideal arithmetic on non-normalized
    operands. Division quotients of magnitude >= 2 are rescaled before guard
    quantization; small quotients retain the existing truncation contract.
    """
    if opcode == 52 and right[0] == 0:
        return *left, True, True
    if opcode == 50:
        exact = Fraction(left[0] * right[0], SCALE * SCALE)
        exponent = left[1] + right[1]
    else:
        exact = Fraction(left[0], right[0])
        exponent = left[1] - right[1]
        while abs(exact) >= 2:
            exact /= 2
            exponent += 1
    scaled = exact * (1 << 37)
    guard = scaled.numerator // scaled.denominator
    fraction, exponent = normalized(Fraction(guard, 1 << 37), exponent)
    return fraction, exponent, not -2048 <= exponent <= 2047, False


def additive(opcode, left, right, compatibility=True):
    """Quantize exact aligned operands, add/subtract, then normalize and round.

    The compatibility contract retains 28 trailing alignment bits until after
    the operation. The manual model retains only two. Neither model emulates
    wrapped 64-bit registers or uses the production overflow/sign expressions.
    """
    difference = left[1] - right[1]
    if compatibility and difference >= 38:
        return *left, False, False
    if compatibility and difference <= -38 and opcode == 48:
        return *right, False, False
    exponent = max(left[1], right[1])
    trailing = 28 if compatibility else 2
    a = left[0] * (1 << trailing) // (1 << (exponent - left[1]))
    b = right[0] * (1 << trailing) // (1 << (exponent - right[1]))
    if compatibility and difference <= -38:
        a = 0
    total = a - b if opcode == 49 else a + b
    # Detect overflow mathematically, without wrapped/sign-bit operations.
    bound = 1 << (35 + trailing)
    if not -bound <= total < bound:
        total //= 2
        exponent += 1
    # Hardware-style reduction precedes cancellation normalization. Replacing
    # this with ideal real addition would lose the compatibility borrow case.
    guard = total // (1 << (trailing - 2))
    fraction, exponent = normalized(Fraction(guard, 1 << 37), exponent)
    return fraction, exponent, not -2048 <= exponent <= 2047, False


def packed(fraction, exponent, low=False):
    bits = fraction % (1 << 36)
    if low:
        # Manual bit 33 is the third least-significant fraction bit.
        bits = (bits & ~3) | (3 if bits & 4 else 0)
    return bits >> 12, ((bits & 4095) << 12) | (exponent % 4096)


def operands():
    fractions = [0, *(HALF + offset for offset in range(8)),
                 3 * (1 << 33), SCALE - 2, SCALE - 1,
                 -SCALE, -SCALE + 1,
                 *(-HALF - offset for offset in range(1, 9))]
    exponents = [(0, 0), (2047, 2047), (-2048, -2048),
                 (2047, -2048), (-2048, 2047), (2047, 0),
                 (-2048, 0), (17, -23)]
    for a, b in itertools.product(fractions, repeat=2):
        for ea, eb in exponents:
            yield (a, ea), (b, eb)
    rng = random.Random(4000)
    for _ in range(512):
        def number():
            if rng.randrange(2):
                fraction = rng.randrange(HALF, SCALE)
            else:
                fraction = rng.randrange(-SCALE, -HALF)
            return fraction, rng.randrange(-2048, 2048)
        yield number(), number()


def conversion_operands():
    scales = [INTEGER_MIN, -4096, -2072, -2049, -2048, -2047,
              -64, -36, -24, -1, 0, 1, 23, 24, 35, 63,
              2024, 2046, 2047, 2048, 4096, INTEGER_MAX]
    integers = [INTEGER_MIN, INTEGER_MIN + 1, -65536, -32769, -32768,
                -3, -2, -1, 0, 1, 2, 3, 32767, 32768, 65535,
                INTEGER_MAX - 1, INTEGER_MAX]
    for integer, scale in itertools.product(integers, scales):
        yield 32, integer, scale
    fractions = [0, 1, -1, HALF, HALF + 1, HALF - 1,
                 -HALF, -HALF - 1, -HALF + 1, SCALE - 1, -SCALE]
    exponents = [-2048, -2047, -64, -28, -1, 0, 1, 23, 24, 25, 35, 63, 2047]
    for fraction, exponent, scale in itertools.product(fractions, exponents, scales):
        yield 53, (fraction, exponent), scale
    # Direct exact values just below, on and above half-integer ties and
    # both signed overflow thresholds, not expectations obtained from C.
    for integer in [INTEGER_MIN - 1, INTEGER_MIN, -2, -1, 0, 1,
                    INTEGER_MAX - 1, INTEGER_MAX, INTEGER_MAX + 1]:
        for offset in [Fraction(-3, 4), Fraction(-1, 2), Fraction(-1, 4),
                       Fraction(0), Fraction(1, 4), Fraction(1, 2), Fraction(3, 4)]:
            value = normalized(Fraction(integer) + offset, 0)
            for scale in (-1, 0, 1):
                yield 53, value, scale


def additive_operands():
    for left, right in operands():
        # Use canonical zero for this normalized-arithmetic comparison, so
        # manual differences are not just unusual zero exponent encodings.
        yield ((0, -2048) if left[0] == 0 else left), \
              ((0, -2048) if right[0] == 0 else right)
    edges = [HALF, HALF + 1, HALF + 3, 3 * (1 << 33), SCALE - 1,
             -SCALE, -SCALE + 1, -HALF - 1]
    differences = [-39, -38, -37, -36, -35, -34, -33, -28, -27, -26,
                   -16, -2, -1, 0, 1, 2, 16, 26, 27, 28,
                   33, 34, 35, 36, 37, 38, 39]
    for a, b, difference in itertools.product(edges, edges, differences):
        yield (a, 0), (b, -difference)
    # Existing workload borrow regression, expressed directly as field values.
    yield (0x40bc319b0, 2), (0x4cccccccd, -2)


def non_normalized_operands():
    # A small signed field, powers of two and their neighbors exercise repeated
    # normalization shifts and bits discarded BEFORE those shifts occur.
    tiny = [1, 2, 3, 7, (1 << 16) - 1, 1 << 16, (1 << 16) + 1,
            (1 << 32) - 1, 1 << 32, (1 << 32) + 1, HALF - 1]
    fractions = [0, *tiny, *(-value for value in tiny),
                 HALF, HALF + 1, SCALE - 1, -HALF, -HALF - 1, -SCALE]
    exponents = [(-2048, -2048), (-2048, 2047), (2047, -2048),
                 (2047, 2047), (0, 0), (0, 1), (0, 27), (0, 37),
                 (0, 38), (0, 39), (38, 0), (39, 0)]

    def non_normalized(fraction):
        return fraction != 0 and -HALF <= fraction < HALF

    for a, b in itertools.product(fractions, repeat=2):
        if not (non_normalized(a) or non_normalized(b)):
            continue
        for ea, eb in exponents:
            for opcode in (48, 49, 50):
                yield opcode, (a, ea), (b, eb)
            yield 52, (a, ea), (b, eb)


def large_division_operands():
    # Boundaries of the previous host-width failure: shifting an integer
    # quotient by 37 lost high bits at 2**27 and mis-set its sign at 2**26.
    numerators = {SCALE - 1, -SCALE}
    for power in (1, 2, 25, 26, 27, 34):
        for offset in (-1, 0, 1):
            magnitude = (1 << power) + offset
            numerators.update((magnitude, -magnitude))
    for numerator, denominator, exponents in itertools.product(
            sorted(numerators), (1, -1, 3, -3, 7, -7),
            ((0, 0), (2047, 0), (-2048, 2047))):
        yield 52, (numerator, exponents[0]), (denominator, exponents[1])


def main():
    # Hand-derived anchors guard the oracle itself against scale, sign and
    # rounding mistakes before comparing anything with the implementation.
    assert arithmetic(50, (HALF, 0), (HALF, 0)) == (HALF, -1, False, False)
    assert arithmetic(50, (-HALF - 1, 0), (3 * (1 << 33), 0)) == \
        (-3 * (1 << 33) - 1, -1, False, False)
    assert arithmetic(50, (HALF + 1, 0), (3 * (1 << 33), 0)) == \
        (3 * (1 << 33) + 2, -1, False, False)
    assert arithmetic(52, (HALF, 0), (3 * (1 << 33), 0)) == \
        ((1 << 36) // 3, 0, False, False)
    assert packed(-SCALE, -1) == (0x800000, 0x000fff)
    assert from_integer(1, 0) == (HALF, 1, False)
    assert from_integer(-1, 0) == (-SCALE, 0, False)
    assert from_integer(0, INTEGER_MAX) == (0, -2048, False)
    assert to_integer((HALF, 0), 0) == (1, False)
    assert to_integer((-SCALE, -1), 0) == (0, False)
    assert to_integer(normalized(Fraction(INTEGER_MIN) - Fraction(1, 2), 0), 0) == \
        (INTEGER_MIN, False)
    assert to_integer(normalized(Fraction(INTEGER_MAX) + Fraction(1, 2), 0), 0) == \
        (99, True)
    assert additive(48, (HALF, 0), (HALF, -1)) == \
        (3 * (1 << 33), 0, False, False)
    assert additive(48, (HALF, 0), (-SCALE, -1)) == (0, -2048, False, False)
    assert additive(49, (HALF + 1, -2048), (HALF, -2048)) == \
        (HALF, -2082, True, False)
    borrow = ((0x40bc319b0, 2), (0x4cccccccd, -2))
    assert packed(*additive(49, *borrow)[:2]) == (0x77dec9, 0x9c6001)
    assert packed(*additive(49, *borrow, compatibility=False)[:2]) == \
        (0x77dec9, 0x9c7001)
    assert additive(49, *borrow) != additive(49, *borrow, compatibility=False)
    # Intermediate truncation is observable: a positive tiny product vanishes,
    # whereas floor truncation leaves a negative one-guard-unit product.
    assert quantized_arithmetic(50, (1, 0), (1, 0)) == (0, -2048, False, False)
    assert quantized_arithmetic(50, (-1, 0), (1, 0)) == \
        (-SCALE, -37, False, False)
    assert arithmetic(50, (1, 0), (1, 0)) == (HALF, -69, False, False)
    assert arithmetic(50, (-1, 0), (1, 0)) == (-SCALE, -70, False, False)
    assert quantized_arithmetic(52, (1, 0), (SCALE - 1, 0)) == \
        (HALF, -34, False, False)
    assert quantized_arithmetic(52, (-1, 0), (SCALE - 1, 0)) == \
        (-5 * (1 << 32), -34, False, False)
    assert quantized_arithmetic(52, (HALF, 0), (1, 0)) == \
        (HALF, 35, False, False)
    assert quantized_arithmetic(52, (-SCALE, 0), (1, 0)) == \
        (-SCALE, 35, False, False)
    assert quantized_arithmetic(52, (2, 0), (1, 0)) == \
        (HALF, 2, False, False)
    assert quantized_arithmetic(52, (-2, 0), (1, 0)) == \
        (-SCALE, 1, False, False)
    # Dominance shortcuts preserve a non-normalized fraction, as section
    # 14.8 specifies, rather than silently normalizing the selected operand.
    assert additive(48, (1, 38), (HALF, 0)) == (1, 38, False, False)
    assert additive(48, (HALF, 0), (-1, 38)) == (-1, 38, False, False)

    requests, expected, descriptions = [], [], []
    manual_differences = 0
    arithmetic_inputs = itertools.chain(
        ((op, left, right, False) for left, right in operands() for op in (50, 52)),
        ((op, left, right, False) for left, right in additive_operands()
         for op in (48, 49)),
        ((op, left, right, True) for op, left, right in itertools.chain(
            non_normalized_operands(), large_division_operands())))
    multiply_divide_count = 0
    additive_count = 0
    non_normalized_count = {48: 0, 49: 0, 50: 0, 52: 0}
    for opcode, left, right, non_normalized in arithmetic_inputs:
        if opcode in (48, 49):
            outcome = additive(opcode, left, right)
            if not non_normalized:
                additive_count += 8
                manual_differences += outcome != additive(opcode, left, right,
                                                         compatibility=False)
        elif non_normalized:
            outcome = quantized_arithmetic(opcode, left, right)
        else:
            outcome = arithmetic(opcode, left, right)
            multiply_divide_count += 8
        if non_normalized:
            non_normalized_count[opcode] += 8
        for reg, low in itertools.product(range(4), range(2)):
            first, second = packed(*left)
            other_first, other_second = packed(*right)
            requests.append(f"{opcode} {reg} {low} {first:x} {second:x} "
                            f"{other_first:x} {other_second:x}\n")
            fraction, exponent, overflow, zero = outcome
            words = [0x111111 * (index + 1) for index in range(4)]
            words[(reg + 3) % 4], words[reg] = \
                (first, second) if zero else packed(fraction, exponent, bool(low))
            expected.append([fraction, exponent, int(overflow), int(zero),
                             *words, low, int(overflow), 0,
                             (1 << 21) if overflow else 0, 34])
            descriptions.append((opcode, reg, low, left, right))
    arithmetic_count = len(expected)
    for opcode, operand, scale in conversion_operands():
        for reg, low in itertools.product(range(4), range(2)):
            if opcode == 32:
                first, second = 0x654321, operand % (1 << 24)
                fraction, exponent, overflow = from_integer(operand, scale)
                helper = [fraction, exponent, int(overflow), 0]
            else:
                first, second = packed(*operand)
                integer, overflow = to_integer(operand, scale)
                helper = [integer, 0, int(overflow), 0]
            scale_bits = scale % (1 << 24)
            requests.append(f"{opcode} {reg} {low} {first:x} {second:x} "
                            f"{scale_bits:x} 0\n")
            words = [0x111111 * (index + 1) for index in range(4)]
            previous = (reg + 3) % 4
            words[previous], words[reg] = first, second
            index = next(index for index in range(1, 4) if index not in (previous, reg))
            words[index] = scale_bits
            if opcode == 32:
                words[previous], words[reg] = packed(fraction, exponent)
            elif not overflow:
                words[reg] = integer % (1 << 24)
            expected.append([*helper, *words, low, int(overflow), 0,
                             (1 << (21 if opcode == 32 else 22)) if overflow else 0,
                             34])
            descriptions.append((opcode, reg, low, operand, scale))
    result = subprocess.run([str(pathlib.Path(sys.argv[1]).resolve())],
                            input="".join(requests), text=True,
                            capture_output=True, timeout=60, check=True)
    assert not result.stderr, result.stderr
    lines = result.stdout.splitlines()
    assert len(lines) == len(expected), (len(lines), len(expected))
    for description, want, line in zip(descriptions, expected, lines):
        got = list(map(int, line.split()))
        assert got == want, (description, "expected", want, "actual", got)
    assert manual_differences > 0
    print(f"Independent FP oracle: {multiply_divide_count} FM/FD, "
          f"{additive_count} FA/FS and "
          f"{len(expected) - arithmetic_count} CI/CF vectors passed; "
          f"{sum(non_normalized_count.values())} non-normalized vectors "
          f"{non_normalized_count}; "
          f"{manual_differences} additive operand/operation cases differ from "
          "the literal 38-bit manual model")


if __name__ == "__main__":
    main()
