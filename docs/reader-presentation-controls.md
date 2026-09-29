# Reader presentation controls

The reader now keeps presentation policy in persisted settings and uses the same values for pagination and rendering.

## Controls

- **Margins**: narrow, normal, wide.
- **Paragraph gap**: normal or wide. A wide gap inserts one additional visual line after explicit paragraph breaks.
- **Alignment**: left, center, right. Alignment is computed per wrapped visual line.
- **Page turn**: normal or reversed. Reversed mode swaps left/right physical navigation, PAGE_PREV/PAGE_NEXT, and the left/right reader tap zones while leaving the center menu zone unchanged.

All controls are persisted in NVS, exposed from Settings and Reader Menu, and included in pagination-cache identity where they affect page geometry.

## E-paper UI behavior

Settings and Reader Menu use a scrolling visible window when the number of rows exceeds the available panel height. This preserves the normal row/touch-target height instead of squeezing all entries onto the screen.
