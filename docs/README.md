# Docs

Statut documentaire : `IMPLEMENTE`

## Role

Point d'entree du corpus documentaire actif.

## Perimetre

### Socle consolide
1. `00_documentation_consolidee_index.md`
2. `01_core_web_cloud_sync.md`
3. `02_ui_qml.md`
4. `03_photon_viewer_formats.md`

### Sections specialisees
- `Anycubic Cloud Client/README.md`
- `Photon Viewer/README.md`
- `MQTT/README.md`
- `i18n/README.md`
- `photon_files/README.md`

## Convention de numerotation

- socle `00..03`: numerotation **transverse** au corpus principal;
- sections: numerotation **locale** (ou transverse UI partagee) declaree dans leur README.

## Profils de lecture

- lecture transverse (architecture/cadrage): suivre le socle `00..03`;
- lecture specialisee (implementation par domaine): entrer par le README de section concernee (`MQTT`, `i18n`, `Anycubic Cloud Client`, `Photon Viewer`);
- lecture annexe (verification/inventaire): consulter uniquement les documents marques `SNAPSHOT` dans les sections.

## Regle

Le socle `00..03` porte la reference transverse.
Les sections specialisent par domaine sans dupliquer le socle.
