export type IdStatus = 'valid' | 'invalid' | 'duplicate' | 'empty';

const ID_REGEX = /^[a-z0-9][a-z0-9-]*[a-z0-9]$|^[a-z0-9]$/;

/**
 * Validate an ID string against project rules:
 * - Only [a-z0-9-] characters
 * - No leading/trailing hyphens
 * - No dots (reserved for id.channel)
 * - No spaces, no uppercase
 * - Not empty
 *
 * existingIds: list of IDs already in use (for uniqueness check).
 * currentIndex: index of the item being edited (excluded from duplicate check).
 */
export function validateId(
  id: string,
  existingIds: string[],
  currentIndex?: number
): IdStatus {
  if (!id || id.trim().length === 0) return 'empty';

  if (!ID_REGEX.test(id)) return 'invalid';

  const isDuplicate = existingIds.some(
    (existing, i) => i !== currentIndex && existing.toLowerCase() === id.toLowerCase()
  );
  if (isDuplicate) return 'duplicate';

  return 'valid';
}

/** Normalize a string into a valid ID (best-effort conversion). */
export function toValidId(input: string): string {
  return input
    .toLowerCase()
    .replace(/\s+/g, '-')
    .replace(/\./g, '-')
    .replace(/[^a-z0-9-]/g, '')
    .replace(/-{2,}/g, '-')
    .replace(/^-+|-+$/g, '');
}
