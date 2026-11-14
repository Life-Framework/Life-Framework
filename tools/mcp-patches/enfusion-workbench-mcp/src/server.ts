import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { mkdirSync } from "node:fs";
import { dirname } from "node:path";
import { registerApiSearch } from "./tools/api-search.js";
import { registerComponentSearch } from "./tools/component-search.js";
import { registerWikiSearch } from "./tools/wiki-search.js";
import { registerWikiRead } from "./tools/wiki-read.js";
import { registerProject } from "./tools/project.js";
import { registerScriptCreate } from "./tools/script-create.js";
import { registerPrefab } from "./tools/prefab.js";
import { registerMod } from "./tools/mod.js";
import { registerConfigCreate } from "./tools/config-create.js";
import { registerServerConfig } from "./tools/server-config.js";
import { registerLayoutCreate } from "./tools/layout-create.js";
import { registerCreateModPrompt } from "./prompts/create-mod.js";
import { registerModifyModPrompt } from "./prompts/modify-mod.js";
import { registerClassResource } from "./resources/class-resource.js";
import { registerPatternResource } from "./resources/pattern-resource.js";
import { registerGroupResource } from "./resources/group-resource.js";
import { SearchEngine } from "./index/search-engine.js";
import { PatternLibrary } from "./patterns/loader.js";
import { WorkbenchClient } from "./workbench/client.js";
import { registerWbLaunch } from "./tools/wb-launch.js";
import { registerWbConnect } from "./tools/wb-connect.js";
import { registerWbDiagnose } from "./tools/wb-diagnose.js";
import { registerWbReload } from "./tools/wb-reload.js";
import { registerWbEditorTools } from "./tools/wb-editor.js";
import { registerWbExecuteAction } from "./tools/wb-execute-action.js";
import { registerWbEntityTools } from "./tools/wb-entities.js";
import { registerWbComponent } from "./tools/wb-components.js";
import { registerWbTerrain } from "./tools/wb-terrain.js";
import { registerWbLayers } from "./tools/wb-layers.js";
import { registerWbResources } from "./tools/wb-resources.js";
import { registerWbPrefabs } from "./tools/wb-prefabs.js";
import { registerWbClipboard } from "./tools/wb-clipboard.js";
import { registerWbScriptEditor } from "./tools/wb-script-editor.js";
import { registerWbLocalization } from "./tools/wb-localization.js";
import { registerWbProjects } from "./tools/wb-projects.js";
import { registerWbValidate } from "./tools/wb-validate.js";
import { registerWbState } from "./tools/wb-state.js";
import { registerWbCapture } from "./tools/wb-capture.js";
import { registerWbPreview } from "./tools/wb-preview.js";
import { registerGameBrowse } from "./tools/game-browse.js";
import { registerGameRead } from "./tools/game-read.js";
import { registerAssetSearch } from "./tools/asset-search.js";
import { registerGameDuplicate } from "./tools/game-duplicate.js";
import { registerWbEntityDuplicate } from "./tools/wb-entity-duplicate.js";
import { registerWorkshopInfo } from "./tools/workshop-info.js";
import { registerScenarioTools } from "./tools/wb-scenario.js";
import { registerScenarioCreate } from "./tools/scenario-create.js";
import { registerAnimationGraph } from "./tools/animation-graph.js";
import { registerWbKnowledge } from "./tools/wb-knowledge.js";
import { registerBuildingSetup } from "./tools/building-setup.js";
import { openProjectIndex } from "./project-index/migrate.js";
import { crawl, type CrawlSource } from "./project-index/crawler.js";
import { ProjectIndex } from "./project-index/project-index.js";
import { ProjectWatcher } from "./watch/project-watcher.js";
import { registerResolveGuid } from "./tools/resolve-guid.js";
import { registerFindReferences } from "./tools/find-references.js";
import { registerProjectIndexStatus } from "./tools/project-index-status.js";
import { registerFindUnusedResources } from "./tools/find-unused-resources.js";
import { registerFindBrokenRefs } from "./tools/find-broken-refs.js";
import { registerInheritanceChain } from "./tools/inheritance-chain.js";
import { registerListResources } from "./tools/list-resources.js";
import { registerListDependencies } from "./tools/list-dependencies.js";
import { registerLogsList } from "./tools/logs-list.js";
import { registerLogsTail } from "./tools/logs-tail.js";
import { registerLogsFilter } from "./tools/logs-filter.js";
import { registerLogsSummarizeErrors } from "./tools/logs-summarize-errors.js";
import { registerServerValidateConfig } from "./tools/server-validate-config.js";
import { registerWorldComposeSummary } from "./tools/world-compose-summary.js";
import { registerWorldValidateRefs } from "./tools/world-validate-refs.js";
import { registerWorldDiff } from "./tools/world-diff.js";
import { registerScenarioInspect } from "./tools/scenario-inspect.js";
import { registerScenarioDiff } from "./tools/scenario-diff.js";
import { registerWorkshopValidateManifest } from "./tools/workshop-validate-manifest.js";
import { registerWorkshopCheckDeps } from "./tools/workshop-check-deps.js";
import { registerWbValidateScripts } from "./tools/wb-validate-scripts.js";
import { registerTextureRecolor } from "./tools/texture-recolor.js";
import { registerWbCliRun } from "./tools/wb-cli-run.js";
import { registerWbBuildData } from "./tools/wb-build-data.js";
import { registerProjectValidate } from "./tools/project-validate.js";
import { registerMaterialInspect } from "./tools/material-inspect.js";
import { registerMaterialFindUnusedTextures } from "./tools/material-find-unused-textures.js";
import { registerMaterialDiff } from "./tools/material-diff.js";
import { registerUiLayoutInspect } from "./tools/ui-layout-inspect.js";
import { registerUiLocalizationAudit } from "./tools/ui-localization-audit.js";
import { registerUiLayoutValidate } from "./tools/ui-layout-validate.js";
import { registerUiExtractStrings } from "./tools/ui-extract-strings.js";
import { registerUiStylesInspect } from "./tools/ui-styles-inspect.js";
import { registerParticleInspect } from "./tools/particle-inspect.js";
import { registerAssetOrphanScan } from "./tools/asset-orphan-scan.js";
import { registerRefactorReplaceGuid } from "./tools/refactor-replace-guid.js";
import { registerRefactorRenameProjectId } from "./tools/refactor-rename-project-id.js";
import { registerRefactorNormalizeDependencies } from "./tools/refactor-normalize-dependencies.js";
import { registerRefactorRemoveUnused } from "./tools/refactor-remove-unused.js";
import { registerRefactorMoveResourcePath } from "./tools/refactor-move-resource-path.js";
import { registerScriptAnalyze } from "./tools/script-analyze.js";
import { registerScriptOverrides } from "./tools/script-overrides.js";
import { registerScriptFindRpcHandlers } from "./tools/script-find-rpc-handlers.js";
import { registerScriptExtractInterface } from "./tools/script-extract-interface.js";
import { registerScriptClassHierarchy } from "./tools/script-class-hierarchy.js";
import { registerScriptLint } from "./tools/script-lint.js";
import { registerScriptFormat } from "./tools/script-format.js";
import { registerRefactorMergeDuplicateGuids } from "./tools/refactor-merge-duplicate-guids.js";
import { registerTerrainInspect } from "./tools/terrain-inspect.js";
import { registerTerrainNavmeshStatus } from "./tools/terrain-navmesh-status.js";
import { registerTerrainRoadExportGraph } from "./tools/terrain-road-export-graph.js";
import { registerScenarioCloneArea } from "./tools/scenario-clone-area.js";
import { registerScenarioApplyTemplate } from "./tools/scenario-apply-template.js";
import { registerFactionCreate } from "./tools/faction-create.js";
import { registerFactionListUnits } from "./tools/faction-list-units.js";
import { registerGmSpawnListExport } from "./tools/gm-spawn-list-export.js";
import { registerAnimationFindUnusedClips } from "./tools/animation-find-unused-clips.js";
import { registerWeaponPoseLint } from "./tools/weapon-pose-lint.js";
import { registerServerLaunch } from "./tools/server-launch.js";
import { registerServerModList } from "./tools/server-mod-list.js";
import { registerServerScenarioPicker } from "./tools/server-scenario-picker.js";
import { registerServerHealthProbe } from "./tools/server-health-probe.js";
import { registerServerStop } from "./tools/server-stop.js";
import { registerMissionSetupPrompt } from "./prompts/mission-setup.js";
import { registerCharacterAnimPipelineGuidePrompt } from "./prompts/character-anim-pipeline-guide.js";
import { logger } from "./utils/logger.js";
import { existsSync } from "node:fs";
import type { Config } from "./config.js";

export function registerTools(server: McpServer, config: Config): void {
  const searchEngine = new SearchEngine(config.dataDir);
  const patterns = new PatternLibrary(config.patternsDir);

  // Phase 0 tools
  registerApiSearch(server, searchEngine);
  registerComponentSearch(server, searchEngine);
  registerWikiSearch(server, searchEngine);
  registerWikiRead(server, searchEngine);
  registerProject(server, config);

  // Phase 1 tools
  registerMod(server, config, searchEngine, patterns);
  registerScriptCreate(server, config, searchEngine);
  registerPrefab(server, config);

  // Phase 3 tools
  registerConfigCreate(server, config);
  registerServerConfig(server, config);
  registerLayoutCreate(server, config);

  // Workbench Live Control tools (Phase 4)
  const wbClient = new WorkbenchClient(config.workbenchHost, config.workbenchPort, config);
  registerWbLaunch(server, config, wbClient);
  registerWbConnect(server, wbClient);
  registerWbDiagnose(server, wbClient);
  registerWbReload(server, wbClient);
  registerWbEditorTools(server, wbClient);
  registerWbExecuteAction(server, wbClient);
  registerWbEntityTools(server, wbClient);
  registerWbComponent(server, wbClient);
  registerWbTerrain(server, wbClient);
  registerWbLayers(server, wbClient);
  registerWbResources(server, wbClient);
  registerWbPrefabs(server, wbClient);
  registerWbClipboard(server, wbClient);
  registerWbScriptEditor(server, wbClient);
  registerWbLocalization(server, wbClient);
  registerWbProjects(server, wbClient);
  registerWbValidate(server, wbClient, config);
  registerWbState(server, wbClient);
  registerWbCapture(server, wbClient, config);
  registerWbPreview(server, wbClient, config);
  registerScenarioTools(server, wbClient);
  registerScenarioCreate(server, config);

  // Base game access tools
  registerGameBrowse(server, config);
  registerGameRead(server, config);
  registerAssetSearch(server, config);
  registerGameDuplicate(server, config, wbClient);
  registerWbEntityDuplicate(server, config, wbClient);
  registerWorkshopInfo(server, config);
  registerAnimationGraph(server, config);
  registerWbKnowledge(server);
  registerBuildingSetup(server, config);
  registerTextureRecolor(server, config, wbClient);

  // Project-index tools (L1 — Goldwep fork addition)
  // mkdir the parent so better-sqlite3 doesn't fail opening the .db.
  mkdirSync(dirname(config.projectIndexPath), { recursive: true });
  const projectIndexDb = openProjectIndex(config.projectIndexPath);

  // L2-2: multi-source crawl at startup + watchers per source.
  // Sources that don't exist on disk are silently skipped — the crawler is
  // opportunistic and chokidar would error on a missing root.
  const sources: CrawlSource[] = [];
  if (existsSync(config.projectPath)) {
    sources.push({ path: config.projectPath, kind: "user" });
  }
  if (config.workshopPath && existsSync(config.workshopPath)) {
    sources.push({ path: config.workshopPath, kind: "workshop" });
  }
  if (existsSync(config.corePath)) {
    sources.push({ path: config.corePath, kind: "core" });
  }

  // Audit-fix C-1: defer the initial crawl off the synchronous startup path
  // so server.connect() can complete the MCP handshake within the client's
  // timeout window (Claude Code / Cursor use ~30s). For a multi-source crawl
  // spanning workshop subscriptions the walk can take many seconds; doing
  // it sync used to risk handshake-timeout shutdowns. Watchers below still
  // start synchronously — they're event-driven and don't block.
  if (sources.length > 0) {
    setImmediate(() => {
      logger.info(`[server] initial crawl (async): ${sources.length} source(s)`);
      try {
        const crawlResult = crawl(projectIndexDb, sources);
        logger.info(
          `[server] initial crawl done: ${crawlResult.projectsIndexed}/${crawlResult.projectsFound} projects, ` +
            `${crawlResult.files.filesScanned} files, ${crawlResult.refs.totalExtracted} refs`,
        );
      } catch (e) {
        logger.warn(
          `[server] initial crawl failed (continuing): ${e instanceof Error ? e.message : String(e)}`,
        );
      }
    });
  } else {
    logger.info("[server] no project-index sources resolved — index will be empty");
  }

  // Watchers per source. chokidar handles recursive walking. Extensions are
  // filtered inside the watcher; .pak / .rdb / .edds are ignored automatically.
  const watchers: ProjectWatcher[] = [];
  for (const source of sources) {
    const watcher = new ProjectWatcher(projectIndexDb, source.path, source.kind);
    try {
      watcher.start();
      watchers.push(watcher);
    } catch (e) {
      logger.warn(
        `[server] watcher start failed for ${source.path} (continuing): ${e instanceof Error ? e.message : String(e)}`,
      );
    }
  }

  // Clean shutdown: stop watchers + close DB on SIGINT/SIGTERM so file
  // handles release (matters on Windows). One-shot guard so repeated signals
  // don't double-stop the watchers (chokidar's close() handles re-entry but
  // we want a clean exit code regardless).
  let shuttingDown = false;
  const handleShutdown = (): void => {
    if (shuttingDown) return;
    shuttingDown = true;
    logger.info(`[server] shutting down — stopping ${watchers.length} watcher(s)`);
    Promise.all(watchers.map((w) => w.stop()))
      .catch((e) => logger.debug(`[server] watcher stop error: ${e}`))
      .finally(() => {
        try {
          projectIndexDb.close();
        } catch {
          /* best-effort */
        }
        process.exit(0);
      });
  };
  process.on("SIGINT", handleShutdown);
  process.on("SIGTERM", handleShutdown);

  registerResolveGuid(server, projectIndexDb);
  registerFindReferences(server, projectIndexDb);
  registerProjectIndexStatus(server, projectIndexDb);

  // L2-4: five reverse-query tools backed by the L2-3 ProjectIndex wrapper.
  const projectIndex = new ProjectIndex(projectIndexDb);
  registerFindUnusedResources(server, projectIndex);
  registerFindBrokenRefs(server, projectIndex);
  registerInheritanceChain(server, projectIndex);
  registerListResources(server, projectIndex);
  registerListDependencies(server, projectIndex);

  // L3-4: logs cluster — pure-FS scan of Workbench/game log session dirs.
  registerLogsList(server, config);
  registerLogsTail(server, config);
  registerLogsFilter(server, config);
  registerLogsSummarizeErrors(server, config);

  // L3-3: server.json validator (uses RedactedServerConfig from L3-2).
  registerServerValidateConfig(server, config);

  // L3-5: world / scenario inspection (pure FS).
  registerWorldComposeSummary(server);
  registerWorldValidateRefs(server, projectIndex);
  registerWorldDiff(server);
  registerScenarioInspect(server);
  registerScenarioDiff(server);

  // L3-6: workshop pre-flight.
  registerWorkshopValidateManifest(server);
  registerWorkshopCheckDeps(server, projectIndex);

  // L3-7: Workbench CLI thin-wraps.
  registerWbValidateScripts(server, config);
  registerWbCliRun(server, config);
  registerWbBuildData(server, config);

  // L3-8: consolidated validator dispatcher.
  registerProjectValidate(server);

  // L4-3: material tools (material_inspect needs config for path resolution).
  registerMaterialInspect(server, projectIndex, config);
  registerMaterialFindUnusedTextures(server, projectIndex, config);
  registerMaterialDiff(server);

  // L4-4: UI tools (5).
  registerUiLayoutInspect(server);
  registerUiLocalizationAudit(server);
  registerUiLayoutValidate(server, projectIndex);
  registerUiExtractStrings(server);
  registerUiStylesInspect(server);

  // L4-5: particle inspect + cross-cutting asset orphan scan.
  registerParticleInspect(server, projectIndex);
  registerAssetOrphanScan(server, projectIndex);

  // L5-1: first resource refactor primitive — GUID-replace across project.
  registerRefactorReplaceGuid(server, projectIndexDb, projectIndex, config);

  // L5-3: surgical .gproj ID rename.
  registerRefactorRenameProjectId(server);

  // L5-4: Dependencies block sort+dedupe+validate.
  registerRefactorNormalizeDependencies(server, projectIndex);

  // L5-6: dry-run removal-script generator for unused resources.
  registerRefactorRemoveUnused(server, projectIndex);

  // L5-2: rename a resource file + update every {GUID}<oldPath> ref.
  registerRefactorMoveResourcePath(server, projectIndexDb, projectIndex);

  // L6-3: first script tool using the L6-1/L6-2 mini-parser.
  registerScriptAnalyze(server);

  // L6-4: walks .c files for `modded class` chains.
  registerScriptOverrides(server);

  // L6-9: find [RPC(...)] / [RplProp] / custom-attribute methods.
  registerScriptFindRpcHandlers(server);

  // L6-8: emit a class's public-facing surface.
  registerScriptExtractInterface(server);

  // L6-7: walk class inheritance + modded chains as ASCII tree.
  registerScriptClassHierarchy(server);

  // L6-5: static analysis with 5 rules ported from BI formatter plugin.
  registerScriptLint(server);

  // L6-6: safe-subset formatter (trailing ws, blank runs, EOF newline).
  registerScriptFormat(server);

  // L5-5: diagnose duplicate-GUID collisions (closes L5).
  registerRefactorMergeDuplicateGuids(server);

  // L7-3: first live-Workbench tool — degrades to clear error if the
  // EMCP_WB_Terrain.c Enforce handler isn't deployed yet (see docs/L7-PLAN.md).
  registerTerrainInspect(server, wbClient);

  // L7-3a: navmesh tile coverage query. Handler placeholder shipped;
  // wired to actual NavmeshWorldComponent next session.
  registerTerrainNavmeshStatus(server, wbClient);

  // L7-3b: road graph extraction. Placeholder; wires to RoadNetworkManager next session.
  registerTerrainRoadExportGraph(server, wbClient);

  // L8 Wave 1 — scenario authoring + faction + GM spawn catalog (pure-FS).
  // scenario tools now take `config` to enforce projectPath containment
  // (audit-fix Sec H-3).
  registerScenarioCloneArea(server, config);
  registerScenarioApplyTemplate(server, config);
  registerFactionCreate(server, config);
  registerFactionListUnits(server, config);
  registerGmSpawnListExport(server, config);

  // L8 Wave 2 — animation extensions + server cluster.
  // server_stop added (audit-fix Arch C-2) to terminate the dedicated
  // server spawned by server_launch — closes the orphan-process gap.
  registerAnimationFindUnusedClips(server, config, projectIndex);
  registerWeaponPoseLint(server);
  registerServerLaunch(server, config);
  registerServerStop(server, config);
  registerServerModList(server, projectIndex);
  registerServerScenarioPicker(server, config, projectIndex);
  registerServerHealthProbe(server);

  // L8 Wave 3 — interactive prompts (per critic: not tools).
  registerMissionSetupPrompt(server);
  registerCharacterAnimPipelineGuidePrompt(server);

  // MCP Prompts
  registerCreateModPrompt(server, patterns);
  registerModifyModPrompt(server);

  // MCP Resources
  registerClassResource(server, searchEngine);
  registerPatternResource(server, patterns);
  registerGroupResource(server, searchEngine);
}
