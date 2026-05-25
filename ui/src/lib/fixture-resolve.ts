import type { EditorFixture, EditorBank, Channel } from './api';

/**
 * Resolve the effective channels for a fixture by following
 * template (bank:fixture) or copy_from references.
 */
export function resolveFixtureChannels(
  fix: EditorFixture,
  fixtures: EditorFixture[],
  banks: EditorBank[]
): Channel[] {
  if (fix.channels.length > 0) return fix.channels;
  if (fix.template) {
    const [bankId, tplId] = fix.template.split(':', 2);
    const bank = banks.find(b => b.id === bankId);
    const tpl = bank?.fixtures.find(f => f.id === tplId);
    if (tpl) return tpl.channels;
  }
  if (fix.copy_from) {
    const src = fixtures.find(f => f.id === fix.copy_from);
    if (src) return resolveFixtureChannels(src, fixtures, banks);
  }
  return [];
}

/**
 * Resolve the effective channel count for a fixture.
 */
export function resolveChannelCount(
  fix: EditorFixture,
  fixtures: EditorFixture[],
  banks: EditorBank[]
): number {
  if (fix.channel_count > 0) return fix.channel_count;
  return resolveFixtureChannels(fix, fixtures, banks).length;
}
