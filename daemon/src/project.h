/*
 * project.h - Project loading and mapping orchestration
 *
 * A project defines the complete fixture patch and scene mapping.
 * spark_project_load() resets all fixture/scene state, loads definitions
 * from a project file (or hardcoded fallback), and resolves scenes.
 *
 * This is the only module that will know about YAML parsing once
 * libyaml is wired in. Fixture and scene modules remain format-agnostic.
 */
#ifndef SPARK_PROJECT_H
#define SPARK_PROJECT_H

int spark_project_load(const char *path);

#endif
