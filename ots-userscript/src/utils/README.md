# Utils Module

Shared utility functions and helpers used throughout the userscript.

## Files

### `dom.ts`
DOM-related utilities for waiting for elements and game initialization.

**Exports:**
- `waitForElement(selector, timeout?)` - Wait for a DOM element to appear
- `waitForGameInstance()` - Wait for the game instance to be available

### `logger.ts`
Centralized logging utilities for consistent console output across the codebase.

**Exports:**
- `createLogger(scope)` - Create a scoped logger with prefix

**Usage:**
```typescript
import { createLogger } from '../utils'

const logger = createLogger('MyModule')

logger.log('Regular message')        // [MyModule] Regular message
logger.warn('Warning message')        // [MyModule] Warning message
logger.error('Error message')         // [MyModule] Error message
logger.success('Success!')            // [MyModule] ✓ Success!
logger.failure('Failed!')             // [MyModule] ✗ Failed!
logger.debug(condition, 'Debug info') // [MyModule] [DEBUG] Debug info (only if condition is true)
```

**Benefits:**
- Consistent log formatting across modules
- Easy filtering by module name in console
- Success/failure indicators
- Conditional debug logging

### `type-guards.ts`
Reusable type guard functions for runtime type checking with TypeScript type narrowing.

**Exports:**
- `isRecord(value)` - Check if value is a plain object
- `isFunction(value)` - Check if value is a function
- `isString(value)` - Check if value is a string
- `isNumber(value)` - Check if value is a number (not NaN)
- `isBoolean(value)` - Check if value is a boolean
- `isDefined(value)` - Check if value is not null/undefined
- `isArray(value)` - Check if value is an array
- `hasProperty(obj, key)` - Check if object has a specific property
- `isNonEmptyString(value)` - Check if value is a non-empty string
- `isNumberInRange(value, min, max)` - Check if value is a number in range

**Usage:**
```typescript
import { isRecord, isNumber, isDefined } from '../utils'

function processData(data: unknown) {
  if (isRecord(data) && isNumber(data.value)) {
    // TypeScript knows data is Record<string, unknown> and data.value is number
    console.log(data.value * 2)
  }
}
```

**Benefits:**
- Eliminates repetitive type checking code
- Provides proper TypeScript type narrowing
- Makes code more readable and maintainable
- Centralized type checking logic

## Module Pattern

All utilities are re-exported through `index.ts` for clean imports:

```typescript
// ✅ Good: Import from utils module
import { createLogger, isRecord, waitForElement } from '../utils'

// ❌ Avoid: Direct file imports
import { createLogger } from '../utils/logger'
import { isRecord } from '../utils/type-guards'
```

## Design Principles

1. **Pure Functions**: All utilities are side-effect-free (except logger)
2. **Type Safety**: Proper TypeScript types and type guards
3. **Reusability**: Generic, composable functions
4. **Documentation**: Clear JSDoc comments on all exports
5. **Single Responsibility**: Each file has one focused purpose
