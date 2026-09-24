# Mixed text, money, code, and mathematics

This paragraph contains ordinary text, **bold text**, *emphasis*, and an
inline equation $E = mc^2$. Another equation sits inside parentheses
($a^2 + b^2 = c^2$), followed immediately by punctuation: $x = 42$!

## Currency must remain text

The first option costs $50 bucks and $100 buys the second option.
Our budget is $1,250.00, with a $99.95 reserve.

A price range is $50–$100. An explicit escaped price is \$75.

Mixing both: spend $50 now and $100 later, then calculate $50 + 100 = 150$.

## Inline code must remain literal

These are code examples, not equations:

- `$E = mc^2$`
- `$50 bucks and $100`
- `<strong>This must not become bold</strong>`
- `<img src="missing.png" alt="This must not become an image">`
- `&lt;div&gt;`
- `const price = "$100";`

Compare the literal source `$\alpha + \beta$` with the equation $\alpha + \beta$.

## Mathematics in sentences

For a sample $x_1, x_2, \ldots, x_n$, the mean is
$\bar{x} = \frac{1}{n}\sum_{i=1}^{n} x_i$.

The expression $\sqrt{x^2 + y^2}$ measures distance, while
$P(A \mid B) = \frac{P(B \mid A)P(A)}{P(B)}$ expresses conditional probability.

**An important constraint:** $0 \leq p \leq 1$.
*An approximation:* $\pi \approx 3.14159$.

## Lists and tables

1. Start with $x = 3$.
2. Calculate $x^2 = 9$.
3. Apply $f(x) = 2x + 1$, giving $f(3) = 7$.
4. Keep the label `$x$` as literal code.

| Description | Equation | Literal source |
|---|---|---|
| Fraction | $\frac{a+b}{c+d}$ | `$\frac{a+b}{c+d}$` |
| Subscript | $x_{i+1}$ | `$x_{i+1}$` |
| Greek letters | $\alpha + \beta = \gamma$ | `$\alpha + \beta = \gamma$` |
| Price | $50 bucks and $100 | `$50 bucks and $100` |

> A quoted paragraph can contain $f(x) = x^2$.
>
> It can also contain ordinary prices: $50 and $100.

## Display equations between paragraphs

This paragraph comes before a display equation.

$$
\int_0^\infty e^{-x^2}\,dx = \frac{\sqrt{\pi}}{2}
$$

This paragraph comes after it. Inline mathematics should still work:
$\frac{1}{2} + \frac{1}{3} = \frac{5}{6}$.

$$
A =
\begin{pmatrix}
1 & 2 & 3 \\
4 & 5 & 6 \\
7 & 8 & 9
\end{pmatrix}
\qquad
\mathbf{v} =
\begin{pmatrix}
x \\ y \\ z
\end{pmatrix}
$$

## Fenced math

```math
f(x) =
\begin{cases}
x^2, & x \geq 0 \\
-x, & x < 0
\end{cases}
```

Regular code fences must remain code:

```javascript
const budget = "$50 bucks and $100";
const formula = "$E = mc^2$";
const markup = "<strong>literal HTML</strong>";
```

## Recovery after invalid mathematics

This intentionally invalid command may appear as an error:
$\notARealCommand{x}$.

The rest of the document should still render normally.

Final check: **bold**, *italic*, `literal $math$`, $x + y = z$, and $50 cash.
