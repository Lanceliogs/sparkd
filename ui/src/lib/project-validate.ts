import type { EditorFixture, EditorScene, EditorBank } from './api';
import { validateId } from './validate';

export interface Problem {
  tab: 'fixtures' | 'scenes';
  index: number;
  message: string;
}

export function validateProject(
  fixtures: EditorFixture[],
  scenes: EditorScene[],
  banks: EditorBank[]
): Problem[] {
  const problems: Problem[] = [];

  checkFixtureIds(fixtures, problems);
  checkFixtureRefs(fixtures, banks, problems);
  checkAddressOverlaps(fixtures, problems);
  checkSceneIds(scenes, problems);

  return problems;
}

function checkFixtureIds(fixtures: EditorFixture[], problems: Problem[]) {
  const ids = fixtures.map(f => f.id);

  for (let i = 0; i < fixtures.length; i++) {
    const status = validateId(fixtures[i].id, ids, i);
    if (status === 'invalid' || status === 'empty') {
      problems.push({ tab: 'fixtures', index: i, message: `Invalid ID "${fixtures[i].id}"` });
    } else if (status === 'duplicate') {
      problems.push({ tab: 'fixtures', index: i, message: `Duplicate ID "${fixtures[i].id}"` });
    }
  }
}

function checkFixtureRefs(fixtures: EditorFixture[], banks: EditorBank[], problems: Problem[]) {
  const fixtureIds = new Set(fixtures.map(f => f.id.toLowerCase()));

  for (let i = 0; i < fixtures.length; i++) {
    const fix = fixtures[i];

    if (fix.template) {
      const parts = fix.template.split(':', 2);
      if (parts.length === 2) {
        const bank = banks.find(b => b.id.toLowerCase() === parts[0].toLowerCase());
        if (!bank) {
          problems.push({ tab: 'fixtures', index: i, message: `Template references unknown bank "${parts[0]}"` });
        } else {
          const tpl = bank.fixtures.find(f => f.id.toLowerCase() === parts[1].toLowerCase());
          if (!tpl) {
            problems.push({ tab: 'fixtures', index: i, message: `Template references unknown fixture "${parts[1]}" in bank "${parts[0]}"` });
          }
        }
      }
    }

    if (fix.copy_from && !fixtureIds.has(fix.copy_from.toLowerCase())) {
      problems.push({ tab: 'fixtures', index: i, message: `copy_from references unknown fixture "${fix.copy_from}"` });
    }
  }
}

function checkAddressOverlaps(fixtures: EditorFixture[], problems: Problem[]) {
  const ranges: Array<{ index: number; id: string; start: number; end: number }> = [];

  for (let i = 0; i < fixtures.length; i++) {
    const fix = fixtures[i];
    if (fix.start_address > 0 && fix.channel_count > 0) {
      ranges.push({
        index: i,
        id: fix.id,
        start: fix.start_address,
        end: fix.start_address + fix.channel_count - 1,
      });
    }
  }

  for (let i = 0; i < ranges.length; i++) {
    for (let j = i + 1; j < ranges.length; j++) {
      if (ranges[i].start <= ranges[j].end && ranges[j].start <= ranges[i].end) {
        problems.push({
          tab: 'fixtures',
          index: ranges[i].index,
          message: `DMX address overlap with "${ranges[j].id}" (${ranges[i].start}-${ranges[i].end} vs ${ranges[j].start}-${ranges[j].end})`,
        });
        problems.push({
          tab: 'fixtures',
          index: ranges[j].index,
          message: `DMX address overlap with "${ranges[i].id}" (${ranges[j].start}-${ranges[j].end} vs ${ranges[i].start}-${ranges[i].end})`,
        });
      }
    }
  }
}

function checkSceneIds(scenes: EditorScene[], problems: Problem[]) {
  const ids = scenes.map(s => s.id);

  for (let i = 0; i < scenes.length; i++) {
    const status = validateId(scenes[i].id, ids, i);
    if (status === 'invalid' || status === 'empty') {
      problems.push({ tab: 'scenes', index: i, message: `Invalid ID "${scenes[i].id}"` });
    } else if (status === 'duplicate') {
      problems.push({ tab: 'scenes', index: i, message: `Duplicate ID "${scenes[i].id}"` });
    }
  }
}
