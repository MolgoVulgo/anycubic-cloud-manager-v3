# Capture des retours endpoints Anycubic

## Statut document

- Type: `SNAPSHOT` (capture datee, non normative).
- Priorite de reference: voir `Docs/endpoints_reference_policy.md`.
- Ce fichier decrit une execution ponctuelle; il ne remplace pas le contrat endpoint.

## Contexte
- Genere le: `2026-03-06T18:04:27Z`
- Sources endpoints: `Docs/end_points.md`, `Docs/end_points_v2_verifie.md`
- Session utilisee: `accloud/session.json`
- Fichier test upload: `Docs/cube.pwmb`
- Fichier telecharge: `/tmp/anycubic_cube_downloaded.pwmb`
- SHA256 upload: `f1eadfe77d442413dc1c19e98c21c17be045c2905efafbee87af027263ef6a00`
- SHA256 download: `f1eadfe77d442413dc1c19e98c21c17be045c2905efafbee87af027263ef6a00`
- Integrite upload/download: `True`

## Resume
- Endpoints traites: `26`
- Succes: `25`
- Echecs: `0`
- Skippes: `1`
- Filtre rapport: `succes uniquement + send_print_order`

## Skips
- `send_print_order`: Desactive par defaut (action intrusive). Utiliser --allow-send-order.

## Details
### 1. store_quota - OK
- Endpoint: `POST https://cloud-universe.anycubic.com/p/p/workbench/api/work/index/getUserStore`
- Source: `Docs/end_points.md::3.1 Store quota, Docs/end_points_v2_verifie.md`
- HTTP status: `200`
- Business code: `1`
- Duree: `356 ms`
- Request body:
```json
{}
```
- Response JSON:
```json
{
  "code": 1,
  "msg": "操作成功 (Operation successful)",
  "data": {
    "used_bytes": 1810451171,
    "total_bytes": 2147483648,
    "used": "1.69GB",
    "total": "2.00GB",
    "user_file_exists": true
  }
}
```

### 2. device_param_get_list - OK
- Endpoint: `POST https://cloud-universe.anycubic.com/p/p/workbench/api/user/device_param/getList`
- Source: `Docs/end_points.md::2.2 Device params list, Docs/end_points_v2_verifie.md`
- HTTP status: `200`
- Business code: `1`
- Duree: `346 ms`
- Request body:
```json
{
  "page": 1,
  "limit": 20
}
```
- Response JSON:
```json
{
  "code": 1,
  "msg": "操作成功 (Operation successful)",
  "data": [],
  "pageData": {
    "page": 1,
    "page_count": 20,
    "total": 0
  }
}
```

### 3. files - OK
- Endpoint: `POST https://cloud-universe.anycubic.com/p/p/workbench/api/work/index/files`
- Source: `Docs/end_points.md::4.1 Listing fichiers (variante A), Docs/end_points_v2_verifie.md`
- HTTP status: `200`
- Business code: `1`
- Duree: `616 ms`
- Request body:
```json
{
  "page": 1,
  "limit": 20
}
```
- Response JSON:
```json
{
  "code": 1,
  "msg": "请求被接受 (Request accepted)",
  "data": [
    {
      "id": 55089382,
      "user_id": 94829,
      "post_id": 0,
      "filename": "177272599013963800-f2e11d7ff203289695183ad95fe5120d-69a9a6e62218147403a170455f3c8c44.pwsz",
      "time": 1772725993,
      "size": 33463874,
      "status": 1,
      "ip": "82.64.136.10",
      "old_filename": "Beetle_fix2.pwsz",
      "img_status": 1,
      "device_type": "web",
      "file_type": 1,
      "md5": "b574212e123ff9ef2db4ab9bb880a6b0",
      "url": "https://cdn.cloud-universe.anycubic.com/file/2026/03/05/pwsz/177272599013963800-f2e11d7ff203289695183ad95fe5120d-69a9a6e62218147403a170455f3c8c44.pwsz",
      "thumbnail": "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026-03/05/jpg/e7b02b598f6246689c4a4c0af683fc42.jpg",
      "is_delete": 0,
      "update_time": 1772725993,
      "uuid": "",
      "store_type": 2,
      "bucket": "workbentch",
      "region": "us-east-2",
      "path": "file/2026/03/05/pwsz/177272599013963800-f2e11d7ff203289695183ad95fe5120d-69a9a6e62218147403a170455f3c8c44.pwsz",
      "thumbnail_nonce": "",
      "sliceparse_nonce": "69a9a6e91949f33fe1e8bc7e15a857c3",
      "file_extension": "pwsz",
      "name_counts": 0,
      "source_user_upload_id": 0,
      "origin_post_id": 0,
      "stl_user_upload_id": 0,
      "is_official_slice": 0,
      "triangles_count": 0,
      "is_parse": 1,
      "source_type": 0,
      "file_source": 1,
      "user_lock_space_id": 45407080,
      "origin_file_md5": "b574212e123ff9ef2db4ab9bb880a6b0",
      "is_temp_file": 0,
      "official_file_key": "",
      "official_file_id": 0,
      "official_file_type": -1,
      "machine_type": 128,
      "printer_image_id": "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026-03/05/jpg/84648ec4d9a94b7fa29ea69fad5fffbe.jpg",
      "plate_number": 0,
      "desc": "",
      "simplify_model": [],
      "size_x": 0,
      "size_y": 0,
      "size_z": 76.70000457763672,
      "estimate": 8842,
      "material_name": "Resin",
      "printer_names": [
        "Anycubic Photon Mono M7 Pro"
      ],
      "slice_param": {
        "active_resins": [
          "r1"
        ],
        "advanced_control": {
          "bott_0": {
            "down_speed": 1,
            "height": 3,
            "z_up_speed": 1
          },
          "bott_1": {
            "down_speed": 3,
            "height": 4,
            "up_speed": 3
          },
          "multi_state_used": 1,
          "normal_0": {
            "down_speed": 3,
            "height": 3,
            "up_speed": 3
          },
          "normal_1": {
            "down_speed": 6,
            "height": 3,
            "up_speed": 6
          },
          "transition_layercount": 6
        },
        "anti_count": 8,
        "bott_layers": 4,
        "bott_time": 20,
        "bucket_id": "cloud-slice-prod",
        "estimate": 8842,
        "exposure_time": 1.5,
        "image0_id": "cloud/2026-03/05/jpg/84648ec4d9a94b7fa29ea69fad5fffbe.jpg",
        "image_id": "cloud/2026-03/05/jpg/e7b02b598f6246689c4a4c0af683fc42.jpg",
        "intelli_mode": 1,
        "layers": 1534,
        "machine_name": "Anycubic Photon Mono M7 Pro",
        "machine_tid": 0,
        "material_name": "Resin",
        "material_tid": 0,
        "off_time": 0.5,
        "size_x": 0,
        "size_y": 0,
        "size_z": 76.70000457763672,
        "sliced_md5": "b574212e123ff9ef2db4ab9bb880a6b0",
        "supplies_usage": 23.882999420166016,
        "user_resin_code": "10",
        "zdown_speed": 3,
        "zthick": 0.05000000074505806,
        "zup_height": 3,
        "zup_speed": 3,
        "basic_control_param": {
          "zdown_speed": 3,
          "zup_height": 3,
          "zup_speed": 3
        },
        "machine_param": {
          "name": "Anycubic Photon Mono M7 Pro"
        }
      },
      "layer_height": 0.05000000074505806,
      "supplies_usage": 23.882999420166016,
      "gcode_id": 76213911
    },
    {
      "id": 54851926,
      "user_id": 94829,
      "post_id": 0,
      "filename": "177257796630428900-151e2eefbaeb794a059ff9ef240b14f6-69a764ae4a4ab903edf155db732f0375.pwsz",
      "time": 1772577974,
      "size": 90686045,
      "status": 1,
      "ip": "82.64.136.10",
      "old_filename": "01_T3d_skull_cut_v4.pwsz",
      "img_status": 1,
      "device_type": "pc",
      "file_type": 1,
      "md5": "ff08f1feb055fb7711bafcbe0ec55843",
      "url": "https://cdn.cloud-universe.anycubic.com/file/2026/03/04/pwsz/177257796630428900-151e2eefbaeb794a059ff9ef240b14f6-69a764ae4a4ab903edf155db732f0375.pwsz",
      "thumbnail": "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026-03/03/jpg/24d48d956e5343b98c97755a5fca53e8.jpg",
      "is_delete": 0,
      "update_time": 1772577976,
      "uuid": "",
      "store_type": 2,
      "bucket": "workbentch",
      "region": "us-east-2",
      "path": "file/2026/03/04/pwsz/177257796630428900-151e2eefbaeb794a059ff9ef240b14f6-69a764ae4a4ab903edf155db732f0375.pwsz",
      "thumbnail_nonce": "",
      "sliceparse_nonce": "69a764b6dbd1ead0877d6f25a772e58d",
      "file_extension": "pwsz",
      "name_counts": 0,
      "source_user_upload_id": 0,
      "origin_post_id": 0,
      "stl_user_upload_id": 0,
      "is_official_slice": 0,
      "triangles_count": 0,
      "is_parse": 1,
      "source_type": 0,
      "file_source": 6,
      "user_lock_space_id": 45206628,
      "origin_file_md5": "ff08f1feb055fb7711bafcbe0ec55843",
      "is_temp_file": 0,
      "official_file_key": "",
      "official_file_id": 0,
      "official_file_type": -1,
      "machine_type": 128,
      "printer_image_id": "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026-03/03/jpg/c72b205c0094426182682182728da5ea.jpg",
      "plate_number": 0,
      "desc": "",
      "simplify_model": [],
      "size_x": 0,
      "size_y": 0,
      "size_z": 18.05000114440918,
      "estimate": 1933,
      "material_name": "Resin",
      "printer_names": [
        "Anycubic Photon Mono M7 Pro"
      ],
      "slice_param": {
        "active_resins": [
          "Anycubic@standard_resin@ACF@normal_print"
        ],
        "advanced_control": {
          "bott_0": {
            "down_speed": 3,
            "height": 5,
            "z_up_speed": 3
          },
          "bott_1": {
            "down_speed": 3,
            "height": 3,
            "up_speed": 3
          },
          "multi_state_used": 0,
          "normal_0": {
            "down_speed": 3,
            "height": 3,
            "up_speed": 3
          },
          "normal_1": {
            "down_speed": 10,
            "height": 4,
            "up_speed": 10
          },
          "transition_layercount": 15
        },
        "anti_count": 1,
        "bott_layers": 5,
        "bott_time": 25,
        "bucket_id": "cloud-slice-prod",
        "estimate": 1933,
        "exposure_time": 1.7999999523162842,
        "image0_id": "cloud/2026-03/03/jpg/c72b205c0094426182682182728da5ea.jpg",
        "image_id": "cloud/2026-03/03/jpg/24d48d956e5343b98c97755a5fca53e8.jpg",
        "intelli_mode": 1,
        "layers": 361,
        "machine_name": "Anycubic Photon Mono M7 Pro",
        "machine_tid": 0,
        "material_name": "标准树脂",
        "material_tid": 0,
        "off_time": 0.5,
        "rerf_function": {
          "enable": false,
          "partition_exposure_array": []
        },
        "size_x": 0,
        "size_y": 0,
        "size_z": 18.05000114440918,
        "sliced_md5": "ff08f1feb055fb7711bafcbe0ec55843",
        "supplies_usage": 77.69161987304688,
        "user_resin_code": "10",
        "zdown_speed": 6,
        "zthick": 0.05000000074505806,
        "zup_height": 8,
        "zup_speed": 6,
        "basic_control_param": {
          "zdown_speed": 6,
          "zup_height": 8,
          "zup_speed": 6
        },
        "machine_param": {
          "name": "Anycubic Photon Mono M7 Pro"
        }
      },
      "layer_height": 0.05000000074505806,
      "supplies_usage": 77.69161987304688,
      "gcode_id": 75894055
    },
    {
      "id": 54779821,
      "user_id": 94829,
      "post_id": 0,
      "filename": "177254723015150600-969807b12cadb260dc582a3088e12186-69a6ec9e24fdc31a348c1cae7de88f94.pwsz",
      "time": 1772547234,
      "size": 90402006,
      "status": 1,
      "ip": "82.64.136.10",
      "old_filename": "skull-14-24-v3.pwsz",
      "img_status": 1,
      "device_type": "web",
      "file_type": 1,
      "md5": "e1e1f1f440c3d99e93fd990d5aeb612e",
      "url": "https://cdn.cloud-universe.anycubic.com/file/2026/03/03/pwsz/177254723015150600-969807b12cadb260dc582a3088e12186-69a6ec9e24fdc31a348c1cae7de88f94.pwsz",
      "thumbnail": "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026-03/03/jpg/876a7b29d7cc4fa69b9dac60f862294c.jpg",
      "is_delete": 0,
      "update_time": 1772547236,
      "uuid": "",
      "store_type": 2,
      "bucket": "workbentch",
      "region": "us-east-2",
      "path": "file/2026/03/03/pwsz/177254723015150600-969807b12cadb260dc582a3088e12186-69a6ec9e24fdc31a348c1cae7de88f94.pwsz",
      "thumbnail_nonce": "",
      "sliceparse_nonce": "69a6eca2b3f5f5cdfd9bdf3e47840100",
      "file_extension": "pwsz",
      "name_counts": 0,
      "source_user_upload_id": 0,
      "origin_post_id": 0,
      "stl_user_upload_id": 0,
      "is_official_slice": 0,
      "triangles_count": 0,
      "is_parse": 1,
      "source_type": 0,
      "file_source": 1,
      "user_lock_space_id": 45145717,
      "origin_file_md5": "e1e1f1f440c3d99e93fd990d5aeb612e",
      "is_temp_file": 0,
      "official_file_key": "",
      "official_file_id": 0,
      "official_file_type": -1,
      "machine_type": 128,
      "printer_image_id": "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026-03/03/jpg/b584828ab7a341c9885bfde63210857c.jpg",
      "plate_number": 0,
      "desc": "",
      "simplify_model": [],
      "size_x": 0,
      "size_y": 0,
      "size_z": 27.80000114440918,
      "estimate": 2654,
      "material_name": "Resin",
      "printer_names": [
        "Anycubic Photon Mono M7 Pro"
      ],
      "slice_param": {
        "active_resins": [
          "r1"
        ],
        "advanced_control": {
          "bott_0": {
            "down_speed": 1,
            "height": 3,
            "z_up_speed": 1
          },
          "bott_1": {
            "down_speed": 3,
            "height": 4,
            "up_speed": 3
          },
          "multi_state_used": 0,
          "normal_0": {
            "down_speed": 3,
            "height": 3,
            "up_speed": 3
          },
          "normal_1": {
            "down_speed": 6,
            "height": 3,
            "up_speed": 6
          },
          "transition_layercount": 6
        },
        "anti_count": 8,
        "bott_layers": 4,
        "bott_time": 20,
        "bucket_id": "cloud-slice-prod",
        "estimate": 2654,
        "exposure_time": 1.5,
        "image0_id": "cloud/2026-03/03/jpg/b584828ab7a341c9885bfde63210857c.jpg",
        "image_id": "cloud/2026-03/03/jpg/876a7b29d7cc4fa69b9dac60f862294c.jpg",
        "intelli_mode": 1,
        "layers": 556,
        "machine_name": "Anycubic Photon Mono M7 Pro",
        "machine_tid": 0,
        "material_name": "Resin",
        "material_tid": 0,
        "off_time": 0.5,
        "size_x": 0,
        "size_y": 0,
        "size_z": 27.80000114440918,
        "sliced_md5": "e1e1f1f440c3d99e93fd990d5aeb612e",
        "supplies_usage": 104,
        "user_resin_code": "10",
        "zdown_speed": 3,
        "zthick": 0.05000000074505806,
        "zup_height": 3,
        "zup_speed": 3,
        "basic_control_param": {
          "zdown_speed": 3,
          "zup_height": 3,
          "zup_speed": 3
        },
        "machine_param": {
          "name": "Anycubic Photon Mono M7 Pro"
        }
      },
      "layer_height": 0.05000000074505806,
      "supplies_usage": 104,
      "gcode_id": 75799464
    },
    {
      "id": 54138897,
     
... <truncated 61733 chars>
```

### 4. user_files - OK
- Endpoint: `POST https://cloud-universe.anycubic.com/p/p/workbench/api/work/index/userFiles`
- Source: `Docs/end_points.md::4.2 Listing fichiers (variante B), Docs/end_points_v2_verifie.md`
- HTTP status: `200`
- Business code: `1`
- Duree: `507 ms`
- Request body:
```json
{
  "page": 1,
  "limit": 20
}
```
- Response JSON:
```json
{
  "code": 1,
  "msg": "请求被接受 (Request accepted)",
  "data": [
    {
      "id": 55089382,
      "url": "https://cdn.cloud-universe.anycubic.com/file/2026/03/05/pwsz/177272599013963800-f2e11d7ff203289695183ad95fe5120d-69a9a6e62218147403a170455f3c8c44.pwsz",
      "thumbnail": "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026-03/05/jpg/e7b02b598f6246689c4a4c0af683fc42.jpg",
      "file_extension": "pwsz",
      "md5": "b574212e123ff9ef2db4ab9bb880a6b0",
      "size": 33463874,
      "create_time": 1772725993,
      "old_filename": "Beetle_fix2.pwsz",
      "file_type": 1,
      "img_status": 1,
      "status": 1,
      "name_counts": 0,
      "source_user_upload_id": 0,
      "is_parse": 1,
      "thumbnail_nonce": "",
      "sliceparse_nonce": "69a9a6e91949f33fe1e8bc7e15a857c3",
      "origin_post_id": 0,
      "region": "us-east-2",
      "bucket": "workbentch",
      "path": "file/2026/03/05/pwsz/177272599013963800-f2e11d7ff203289695183ad95fe5120d-69a9a6e62218147403a170455f3c8c44.pwsz",
      "simplify_model": [],
      "gcode_id": 76213911
    },
    {
      "id": 54851926,
      "url": "https://cdn.cloud-universe.anycubic.com/file/2026/03/04/pwsz/177257796630428900-151e2eefbaeb794a059ff9ef240b14f6-69a764ae4a4ab903edf155db732f0375.pwsz",
      "thumbnail": "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026-03/03/jpg/24d48d956e5343b98c97755a5fca53e8.jpg",
      "file_extension": "pwsz",
      "md5": "ff08f1feb055fb7711bafcbe0ec55843",
      "size": 90686045,
      "create_time": 1772577974,
      "old_filename": "01_T3d_skull_cut_v4.pwsz",
      "file_type": 1,
      "img_status": 1,
      "status": 1,
      "name_counts": 0,
      "source_user_upload_id": 0,
      "is_parse": 1,
      "thumbnail_nonce": "",
      "sliceparse_nonce": "69a764b6dbd1ead0877d6f25a772e58d",
      "origin_post_id": 0,
      "region": "us-east-2",
      "bucket": "workbentch",
      "path": "file/2026/03/04/pwsz/177257796630428900-151e2eefbaeb794a059ff9ef240b14f6-69a764ae4a4ab903edf155db732f0375.pwsz",
      "simplify_model": [],
      "gcode_id": 75894055
    },
    {
      "id": 54779821,
      "url": "https://cdn.cloud-universe.anycubic.com/file/2026/03/03/pwsz/177254723015150600-969807b12cadb260dc582a3088e12186-69a6ec9e24fdc31a348c1cae7de88f94.pwsz",
      "thumbnail": "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026-03/03/jpg/876a7b29d7cc4fa69b9dac60f862294c.jpg",
      "file_extension": "pwsz",
      "md5": "e1e1f1f440c3d99e93fd990d5aeb612e",
      "size": 90402006,
      "create_time": 1772547234,
      "old_filename": "skull-14-24-v3.pwsz",
      "file_type": 1,
      "img_status": 1,
      "status": 1,
      "name_counts": 0,
      "source_user_upload_id": 0,
      "is_parse": 1,
      "thumbnail_nonce": "",
      "sliceparse_nonce": "69a6eca2b3f5f5cdfd9bdf3e47840100",
      "origin_post_id": 0,
      "region": "us-east-2",
      "bucket": "workbentch",
      "path": "file/2026/03/03/pwsz/177254723015150600-969807b12cadb260dc582a3088e12186-69a6ec9e24fdc31a348c1cae7de88f94.pwsz",
      "simplify_model": [],
      "gcode_id": 75799464
    },
    {
      "id": 54138897,
      "url": "https://cdn.cloud-universe.anycubic.com/file/2026/02/27/pwmb/177220783284085400-ff3ef29af312f3bd93640de6d6bbb323-69a1bed8cd4a1cdf44dbb4197b3f0eb6.pwmb",
      "thumbnail": "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026-02/27/jpg/ccda193f9dbd414e8a847ae6600da42e.jpg",
      "file_extension": "pwmb",
      "md5": "7c3525ac6d04019af180a578a614a4a1",
      "size": 169805182,
      "create_time": 1772207840,
      "old_filename": "catskull-20-20.pwmb",
      "file_type": 1,
      "img_status": 1,
      "status": 1,
      "name_counts": 0,
      "source_user_upload_id": 0,
      "is_parse": 1,
      "thumbnail_nonce": "",
      "sliceparse_nonce": "69a1bee0d56169cb955b7320d527358f",
      "origin_post_id": 0,
      "region": "us-east-2",
      "bucket": "workbentch",
      "path": "file/2026/02/27/pwmb/177220783284085400-ff3ef29af312f3bd93640de6d6bbb323-69a1bed8cd4a1cdf44dbb4197b3f0eb6.pwmb",
      "simplify_model": [],
      "gcode_id": 74970754
    },
    {
      "id": 53978129,
      "url": "https://cdn.cloud-universe.anycubic.com/file/2026/02/26/pwmb/177211808910881300-6675724dde96a1790c8725efe0b7b9f3-69a060491a9190f43a0abd688d8f54a4.pwmb",
      "thumbnail": "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026-02/26/jpg/28a529a3a7de4b4cb58d1f69dd34209c.jpg",
      "file_extension": "pwmb",
      "md5": "512279a1fb7d473153277238449e2617",
      "size": 36476864,
      "create_time": 1772118092,
      "old_filename": "skull_10_50_v3.pwmb",
      "file_type": 1,
      "img_status": 1,
      "status": 1,
      "name_counts": 0,
      "source_user_upload_id": 0,
      "is_parse": 1,
      "thumbnail_nonce": "",
      "sliceparse_nonce": "69a0604c0b1dae4d11222abf307cbd08",
      "origin_post_id": 0,
      "region": "us-east-2",
      "bucket": "workbentch",
      "path": "file/2026/02/26/pwmb/177211808910881300-6675724dde96a1790c8725efe0b7b9f3-69a060491a9190f43a0abd688d8f54a4.pwmb",
      "simplify_model": [],
      "gcode_id": 74757823
    },
    {
      "id": 53490967,
      "url": "https://cdn.cloud-universe.anycubic.com/file/2026/02/23/pwmb/177184865039141900-27fe3cb6658c19a64f5993099ecd668a-699c43ca5f927b04ee228a84a1c6ceb3.pwmb",
      "thumbnail": "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026-02/23/jpg/905181bfbc8e47ec9978d53c8571030f.jpg",
      "file_extension": "pwmb",
      "md5": "583162a7c4d524e3354834181777c6bb",
      "size": 68287039,
      "create_time": 1771848654,
      "old_filename": "raven_skull-12-25-v3.pwmb",
      "file_type": 1,
      "img_status": 1,
      "status": 1,
      "name_counts": 0,
      "source_user_upload_id": 0,
      "is_parse": 1,
      "thumbnail_nonce": "",
      "sliceparse_nonce": "699c43ce274a73a1bec9c80ebd276b56",
      "origin_post_id": 0,
      "region": "us-east-2",
      "bucket": "workbentch",
      "path": "file/2026/02/23/pwmb/177184865039141900-27fe3cb6658c19a64f5993099ecd668a-699c43ca5f927b04ee228a84a1c6ceb3.pwmb",
      "simplify_model": [],
      "gcode_id": 74119262
    },
    {
      "id": 52845680,
      "url": "https://cdn.cloud-universe.anycubic.com/file/2026/02/20/pwmb/177152626437615700-aeb1f4c025f97e710c24142a9245dcd8-699758785bd6928af053b6addc8d3ccd.pwmb",
      "thumbnail": "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026-02/19/jpg/4c3c83551ac5427f80c203062c3b105d.jpg",
      "file_extension": "pwmb",
      "md5": "8a419196a9453074035df6e2868d1a06",
      "size": 98653687,
      "create_time": 1771526269,
      "old_filename": "raven_skull_19_10_v3.pwmb",
      "file_type": 1,
      "img_status": 1,
      "status": 1,
      "name_counts": 0,
      "source_user_upload_id": 0,
      "is_parse": 1,
      "thumbnail_nonce": "",
      "sliceparse_nonce": "6997587d4ec2df856917e0e41cacf93a",
      "origin_post_id": 0,
      "region": "us-east-2",
      "bucket": "workbentch",
      "path": "file/2026/02/20/pwmb/177152626437615700-aeb1f4c025f97e710c24142a9245dcd8-699758785bd6928af053b6addc8d3ccd.pwmb",
      "simplify_model": [],
      "gcode_id": 73300338
    },
    {
      "id": 50419564,
      "url": "https://cdn.cloud-universe.anycubic.com/file/2026/02/06/pwmb/177030801212370200-95f1ae8e547cd0516d130b68e44827e2-6984c1ac1e343b5e82b21b6f346dc3fe.pwmb",
      "thumbnail": "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026-02/05/jpg/900c782a4e7a4893a17f0d83e7f1e7c4.jpg",
      "file_extension": "pwmb",
      "md5": "d5ef6d0f82afa49345955ce90e37a907",
      "size": 5548168,
      "create_time": 1770308016,
      "old_filename": "catskull-20-v2.pwmb",
      "file_type": 1,
      "img_status": 1,
      "status": 1,
      "name_counts": 0,
      "source_user_upload_id": 0,
      "is_parse": 1,
      "thumbnail_nonce": "",
      "sliceparse_nonce": "6984c1b0577aa627f4e1fe29f4d0b3af",
      "origin_post_id": 0,
      "region": "us-east-2",
      "bucket": "workbentch",
      "path": "file/2026/02/06/pwmb/177030801212370200-95f1ae8e547cd0516d130b68e44827e2-6984c1ac1e343b5e82b21b6f346dc3fe.pwmb",
      "simplify_model": [],
      "gcode_id": 70186340
    },
    {
      "id": 50418549,
      "url": "https://cdn.cloud-universe.anycubic.com/file/2026/02/06/pwmb/177030764920253300-4388b1386a3fc688e232bc85cd4ab666-6984c04131730cf826fb47c426a37871.pwmb",
      "thumbnail": "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026-02/05/jpg/472de0c3838e467888a5c41c67a4f1d6.jpg",
      "file_extension": "pwmb",
      "md5": "08dfdcdd8d5abc3a5b0531d647625583",
      "size": 67851100,
      "create_time": 1770307656,
      "old_filename": "catskull_20_25_v1.pwmb",
      "file_type": 1,
      "img_status": 1,
      "status": 1,
      "name_counts": 0,
      "source_user_upload_id": 0,
      "is_parse": 1,
      "thumbnail_nonce": "",
      "sliceparse_nonce": "6984c048debda0b9fd286cb609a427f3",
      "origin_post_id": 0,
      "region": "us-east-2",
      "bucket": "workbentch",
      "path": "file/2026/02/06/pwmb/177030764920253300-4388b1386a3fc688e232bc85cd4ab666-6984c04131730cf826fb47c426a37871.pwmb",
      "simplify_model": [],
      "gcode_id": 70185015
    },
    {
      "id": 50374796,
      "url": "https://cdn.cloud-universe.anycubic.com/file/2026/02/05/pwmb/177028805200599300-d411d7f6e76f199a632236bce72003fa-698473b4017753e62a436410bde449be.pwmb",
      "thumbnail": "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026-02/05/jpg/efb33fbce4f145afbd977a247f4aee7f.jpg",
      "file_extension": "pwmb",
      "md5": "89bad5441253c0dd4503ab49db4f5001",
      "size": 7942290,
      "create_time": 1770288056,
      "old_filename": "catskull-20.pwmb",
      "file_type": 1,
      "img_status": 1,
      "status": 1,
      "name_counts": 0,
      "source_user_upload_id": 0,
      "is_parse": 1,
      "thumbnail_nonce": "",
      "sliceparse_nonce": "698473b891f52f64ac34cd19159d62ac",
      "origin_post_id": 0,
      "region": "us-east-2",
      "bucket": "workbentch",
      "path": "file/2026/02/05/pwmb/177028805200599300-d411d7f6e76f199a632236bce72003fa-698473b4017753e62a436410bde449be.pwmb",
      "simplify_model": [],
      "gcode_id": 70126881
    },
    {
      "id": 49777814,
      "url": "https://cdn.cloud-universe.anycubic.com/file/2026/02/02/pwmb/176996980033983300-352170fc3066014e03152aa4216f8960-697f988852f84beffcfbb54b65c1051f.pwmb",
      "thumbnail": "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026-02/01/jpg/44f58138ae454edc9e36198310d8cbad.jpg",
      "file_extension": "pwmb",
      "md5": "89f52157c6f442ed9772a88732b7cda0",
      "size": 120820316,
      "create_time": 1769969806,
      "old_filename": "cat_skull-grand_5_v4.pwmb",
      "file_type": 1,
      "img_status": 1,
      "status": 1,
      "name_counts": 0,
      "source_user_upload_id": 0,
      "is_parse": 1,
      "thumbnail_nonce": "",
      "sliceparse_nonce": "697f988e51811c2a7f66b1f9d5dc9cad",
      "origin_post_id": 0,
      "region": "us-east-2",
      "bucket": "workbentch",
      "path": "file/2026/02/02/pwmb/176996980033983300-352170fc3066014e03152aa4216f8960-697f988852f84beffcfbb54b65c1051f.pwmb",
      "simplify_model": [],
      "gcode_id": 69368651
    },
    {
      "id": 41156844,
      "url": "https://cdn.cloud-universe.anycubic.com/file/2025/12/16/pwmb/176581749602819300-c64419b2afd4594a80cff7b4e598537f-69403c9806e2eb72e311c1283c3cbd20.pwmb",
      "thumbnail": "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2025-12/15/jpg/0a5b3da58c3746a7b559babbd137adf6.jpg",
      "file_extension": "pwmb",
      "md5": "5ec226efa2f97c4d79bc6fbde52cf4c8",
      "size": 131549693,
      "create_time": 1765817506,
      "old_filename": "raven_skull_grand.pwmb",
      "file_type": 1,
      "img_status": 1,
      "status": 1,
      "name_counts": 0,
      "source_user_upload_id": 0,
      "is_parse": 1,
      "thumbnail_nonce": "",
      "sli
... <truncated 8621 chars>
```

### 5. upload_lock_storage - OK
- Endpoint: `POST https://cloud-universe.anycubic.com/p/p/workbench/api/v2/cloud_storage/lockStorageSpace`
- Source: `Docs/end_points.md::6.1 Lock storage space, Docs/end_points_v2_verifie.md`
- HTTP status: `200`
- Business code: `1`
- Duree: `391 ms`
- Request body:
```json
{
  "name": "cube.pwmb",
  "size": 2292920,
  "is_temp_file": 0
}
```
- Response JSON:
```json
{
  "code": 1,
  "msg": "操作成功 (Operation successful)",
  "data": {
    "id": 45536795,
    "url": "https://workbentch.s3.us-east-2.amazonaws.com/file/2026/03/07/pwmb/177282025691289000-e5bfdb2e8d37a36945c1f6f4f7d9fb76-69ab1720dee047cc80a26046cf46d1ee.pwmb",
    "preSignUrl": "<signed-url-redacted>"
  }
}
```

### 6. upload_put_presigned - OK
- Endpoint: `PUT https://workbentch.s3.us-east-2.amazonaws.com/file/2026/03/07/pwmb/177282025691289000-e5bfdb2e8d37a36945c1f6f4f7d9fb76-69ab1720dee047cc80a26046cf46d1ee.pwmb?<redacted>`
- Source: `Docs/end_points.md::6.2 Upload binaire direct, Docs/end_points_v2_verifie.md`
- HTTP status: `200`
- Duree: `1091 ms`
- Request body:
```json
<binary 2292920 bytes>
```

### 7. upload_register_file - OK
- Endpoint: `POST https://cloud-universe.anycubic.com/p/p/workbench/api/v2/profile/newUploadFile`
- Source: `Docs/end_points.md::6.3 Register uploaded file, Docs/end_points_v2_verifie.md`
- HTTP status: `200`
- Business code: `1`
- Duree: `469 ms`
- Request body:
```json
{
  "user_lock_space_id": 45536795
}
```
- Response JSON:
```json
{
  "code": 1,
  "msg": "操作成功 (Operation successful)",
  "data": {
    "id": 55240781
  }
}
```

### 8. upload_status - OK
- Endpoint: `POST https://cloud-universe.anycubic.com/p/p/workbench/api/work/index/getUploadStatus`
- Source: `Docs/end_points.md::4.6 Statut post-upload, Docs/end_points_v2_verifie.md`
- HTTP status: `200`
- Business code: `1`
- Duree: `350 ms`
- Request body:
```json
{
  "id": 55240781
}
```
- Response JSON:
```json
{
  "code": 1,
  "msg": "操作成功 (Operation successful)",
  "data": {
    "id": 55240781,
    "status": 2,
    "gcode_id": 0
  }
}
```

### 9. rename_uploaded_file - OK
- Endpoint: `POST https://cloud-universe.anycubic.com/p/p/workbench/api/work/index/renameFile`
- Source: `Docs/end_points.md::4.5 Renommage fichier, Docs/end_points_v2_verifie.md`
- HTTP status: `200`
- Business code: `1`
- Duree: `365 ms`
- Request body:
```json
{
  "id": 55240781,
  "name": "cube_api_probe.pwmb"
}
```
- Response JSON:
```json
{
  "code": 1,
  "msg": "修改成功",
  "data": "cube_api_probe.pwmb.pwmb"
}
```

### 10. download_url_for_uploaded_file - OK
- Endpoint: `POST https://cloud-universe.anycubic.com/p/p/workbench/api/work/index/getDowdLoadUrl`
- Source: `Docs/end_points.md::4.3 URL de telechargement signee, Docs/end_points_v2_verifie.md`
- HTTP status: `200`
- Business code: `1`
- Duree: `369 ms`
- Request body:
```json
{
  "id": 55240781
}
```
- Response JSON:
```json
{
  "code": 1,
  "msg": "操作成功 (Operation successful)",
  "data": "<signed-url-redacted>"
}
```

### 11. download_signed_url_get - OK
- Endpoint: `GET https://workbentch.s3.us-east-2.amazonaws.com/file/2026/03/07/pwmb/177282025691289000-e5bfdb2e8d37a36945c1f6f4f7d9fb76-69ab1720dee047cc80a26046cf46d1ee.pwmb?<redacted>`
- Source: `Docs/end_points.md::4.3 URL de telechargement signee, Docs/end_points_v2_verifie.md`
- HTTP status: `200`
- Duree: `937 ms`
- Response texte:
```text
ANYCUBIC4�@��&�&�?,@�@HEADERT��	B��L=�??�A�@@@@@@@��~?���~?��~?$PREVIEW&�x�!!!!!BcccQ�Q�Q�Q�Q�B!!!BBccc��Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�MkB!!!BBcccQ�Q�Q�Q�Q�Q�Q�Q����Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�BB!!!BMkMkMkQ�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q��Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�BB!!!BBMkccQ�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�B!!!!BMkMkMkQ�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�B!!!BBMkMkMkQ�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�MkB!!IJIJMkMkMk����������Q�Q�Q�Q�Q�Q�Q�����������Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�BB!Mk��������������������������������������������������������Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�BBE)U�Ӝ��������������������������������������������������������Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�B!E)U�U�Ӝ��������������������������������������������������������Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�MkBU�U�U��������������������������������������������������������������������Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�BBU�U�U�U�Ӝ������������������������������������������������������������������������Q�Q�Q�Q�Q�Q�Q���������Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�BB�{U�U�U�U�U�Ӝ����������������������������������������������������������������������Q�Q�Q�Q�Q�������������Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�B!�{U�U�U�U�U�U��������������������������������������������������������������������������������������������Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�B�{U�U�U�U�U�U�U����������������������������������������������������������������������������������������������������������Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�MkB�RU�U�U�U�U�U�U�U����������������������������������������������������������������������������������������������������������Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�BB�RU�U�U�U�U�U�U�U�U�U���������������������������������������������������������������������������������������������������������Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�BBE)U�U�U�U�U�U�U�U�U�U�U������������������������������������������������������������������������������������������������������Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�B!E)U�U�U�U�U�U�U�U�U�U�U�U������������������������������������������������������������������������������������������������������Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q��9E)U�U�U�U�U�U�U�U�U�U�U�U�U������������������������������������������������������������������������������������������������������Q�Q���������������Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q����{MkMkcccU�U�U�U�U�U�U�U�U�U�U�U�U���U�������������������������������������������������������������������������������������������������������������������������Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q����{�s�sMkMkccccccIJU�U�U�U�U�U�U�U�U�U�U�U�U�������Ӝ����������������������������������������������������������������������������������������������������������������������Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q����{�s�sMkMkMkMkMkMkMkMkMkccccccIJ�{U�U�U�U�U�U�U�U�U�U�U�U��������������������������������������������������������������������������������������������������������������������������������Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q����{�s�sMkMkMkMkMkMkMkMkMkMkMkMkMkMkccccccc�1�{U�U�U�U�U�U�U�U�U�U�U�U�U������������������������������������������������������������������������������������������������������������������������������Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�����{�s�s�sMkMkMkMkMkMkMkMkMkMkMkMkMkMkMkMkMkMkcccccccc�1�{U�U�U�U�U�U�U�U�U�U�U�U�U��������������������������������������������������������������������������������������������������������������������������������Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�Q�����{�{�s�s�s�s�s�s�s�sMkMkMkMkMkMkMkMkMkMkMkMkMkMkMkMkMkccccccccc�1�RU�U�U�U�U�U�U�U�U�U�U�U�U�������������U�Ӝ��������������������������������������������������������������������������������������������������������������������������Q�Q�Q�Q�Q�Q�Q�Q�Q��{�{�{�s�s�s�s�s�s�s�s�s�s�s�s�s�sMkMkMkMkMkMkMkMkMkMkMkMkMkMkMkMkMkccccccccc��RU�U�U�U�U�U�U�U�U�U�U�U�U������������������ӜӜӜӜӜӜӜӜ��������������������������������������������������������������������������������������������������������Q�Q�����{�{�{�{�{�s�s�s�s�s�s�s�s�s�s�s�s�s�s�sMkMkMkMkMkMkMkMkMkMkMkMkMkMkMkMkMkccccccccccE)U�U�U�U�U�U�U�U�U�U�U�U�U�U������������������ӜӜӜӜӜӜӜӜ����������������������������
... <truncated 14030 chars>
```

### 12. delete_uploaded_file - OK
- Endpoint: `POST https://cloud-universe.anycubic.com/p/p/workbench/api/work/index/delFiles`
- Source: `Docs/end_points.md::4.4 Suppression fichier, Docs/end_points_v2_verifie.md`
- HTTP status: `200`
- Business code: `1`
- Duree: `382 ms`
- Request body:
```json
{
  "idArr": [
    55240781
  ]
}
```
- Response JSON:
```json
{
  "code": 1,
  "msg": "操作成功 (Operation successful)",
  "data": ""
}
```

### 13. upload_unlock_storage - OK
- Endpoint: `POST https://cloud-universe.anycubic.com/p/p/workbench/api/v2/cloud_storage/unlockStorageSpace`
- Source: `Docs/end_points.md::6.4 Unlock storage space, Docs/end_points_v2_verifie.md`
- HTTP status: `200`
- Business code: `1`
- Duree: `360 ms`
- Request body:
```json
{
  "id": 45536795,
  "is_delete_cos": 0
}
```
- Response JSON:
```json
{
  "code": 1,
  "msg": "操作成功 (Operation successful)",
  "data": ""
}
```

### 14. gcode_info - OK
- Endpoint: `GET https://cloud-universe.anycubic.com/p/p/workbench/api/api/work/gcode/info?id=76213911`
- Source: `Docs/end_points.md::5.1 Gcode info, Docs/end_points_v2_verifie.md`
- HTTP status: `200`
- Business code: `1`
- Duree: `350 ms`
- Response JSON:
```json
{
  "code": 1,
  "msg": "连接成功 (Connection successful)",
  "data": {
    "id": 76213911,
    "user_id": 94829,
    "name": "Beetle_fix2.pwsz",
    "key": "",
    "model": 55089382,
    "img": "http://apitest.anycubic.com/api/work/gcode/getgcode_img/id/76213911",
    "slice_param": {
      "active_resins": [
        "r1"
      ],
      "advanced_control": {
        "bott_0": {
          "down_speed": 1.0,
          "height": 3.0,
          "z_up_speed": 1.0
        },
        "bott_1": {
          "down_speed": 3.0,
          "height": 4.0,
          "up_speed": 3.0
        },
        "multi_state_used": 1,
        "normal_0": {
          "down_speed": 3.0,
          "height": 3.0,
          "up_speed": 3.0
        },
        "normal_1": {
          "down_speed": 6.0,
          "height": 3.0,
          "up_speed": 6.0
        },
        "transition_layercount": 6
      },
      "anti_count": 8,
      "bott_layers": 4,
      "bott_time": 20.0,
      "bucket_id": "cloud-slice-prod",
      "estimate": 8842,
      "exposure_time": 1.5,
      "image0_id": "cloud/2026-03/05/jpg/84648ec4d9a94b7fa29ea69fad5fffbe.jpg",
      "image_id": "cloud/2026-03/05/jpg/e7b02b598f6246689c4a4c0af683fc42.jpg",
      "intelli_mode": 1,
      "layers": 1534,
      "machine_name": "Anycubic Photon Mono M7 Pro",
      "machine_tid": 0,
      "material_name": "Resin",
      "material_tid": 0,
      "off_time": 0.5,
      "size_x": 0.0,
      "size_y": 0.0,
      "size_z": 76.70000457763672,
      "sliced_md5": "b574212e123ff9ef2db4ab9bb880a6b0",
      "supplies_usage": 23.882999420166016,
      "user_resin_code": "10",
      "zdown_speed": 3.0,
      "zthick": 0.05000000074505806,
      "zup_height": 3.0,
      "zup_speed": 3.0,
      "basic_control_param": {
        "zdown_speed": 3.0,
        "zup_height": 3.0,
        "zup_speed": 3.0
      },
      "machine_param": {
        "name": "Anycubic Photon Mono M7 Pro"
      },
      "material_type": "Resin",
      "material_unit": "ml"
    },
    "slice_support": null,
    "support_param": null,
    "req": null,
    "description": "",
    "source": "云存储",
    "progress": 100,
    "hollow_param": "",
    "punching_param": "",
    "image_id": "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026-03/05/jpg/e7b02b598f6246689c4a4c0af683fc42.jpg",
    "bucket_id": "workbentch",
    "sliced_id": "",
    "slice_result": {
      "active_resins": [
        "r1"
      ],
      "advanced_control": {
        "bott_0": {
          "down_speed": 1.0,
          "height": 3.0,
          "z_up_speed": 1.0
        },
        "bott_1": {
          "down_speed": 3.0,
          "height": 4.0,
          "up_speed": 3.0
        },
        "multi_state_used": 1,
        "normal_0": {
          "down_speed": 3.0,
          "height": 3.0,
          "up_speed": 3.0
        },
        "normal_1": {
          "down_speed": 6.0,
          "height": 3.0,
          "up_speed": 6.0
        },
        "transition_layercount": 6
      },
      "anti_count": 8,
      "bott_layers": 4,
      "bott_time": 20.0,
      "bucket_id": "cloud-slice-prod",
      "estimate": 8842,
      "exposure_time": 1.5,
      "image0_id": "cloud/2026-03/05/jpg/84648ec4d9a94b7fa29ea69fad5fffbe.jpg",
      "image_id": "cloud/2026-03/05/jpg/e7b02b598f6246689c4a4c0af683fc42.jpg",
      "intelli_mode": 1,
      "layers": 1534,
      "machine_name": "Anycubic Photon Mono M7 Pro",
      "machine_tid": 0,
      "material_name": "Resin",
      "material_tid": 0,
      "off_time": 0.5,
      "size_x": 0.0,
      "size_y": 0.0,
      "size_z": 76.70000457763672,
      "sliced_md5": "b574212e123ff9ef2db4ab9bb880a6b0",
      "supplies_usage": 23.882999420166016,
      "user_resin_code": "10",
      "zdown_speed": 3.0,
      "zthick": 0.05000000074505806,
      "zup_height": 3.0,
      "zup_speed": 3.0,
      "material_type": "Resin",
      "material_unit": "ml"
    },
    "slice_support_pr": null,
    "type": "TYPE_LCD_SLICE_FILE_PARSE_RESP",
    "code": 1000,
    "desc": "SUCCESS",
    "dispatch_id": "",
    "task_id": 0,
    "nonce": "69a9a6e91949f33fe1e8bc7e15a857c3",
    "machine_class": 0,
    "create_time": 1772725993,
    "end_time": 1772725993,
    "timestamp": 1772725993,
    "status": 2,
    "counts": 1,
    "size": 33463874,
    "delete": 0,
    "estimate": 8842,
    "ams_settings": null,
    "plate_info": null,
    "machine_name": "Anycubic Photon Mono M7 Pro"
  }
}
```

### 15. printers_list - OK
- Endpoint: `GET https://cloud-universe.anycubic.com/p/p/workbench/api/work/printer/getPrinters`
- Source: `Docs/end_points.md::8.1 Liste imprimantes, Docs/end_points_v2_verifie.md`
- HTTP status: `200`
- Business code: `1`
- Duree: `377 ms`
- Response JSON:
```json
{
  "code": 1,
  "msg": "请求被接受 (Request accepted)",
  "data": [
    {
      "id": 525668,
      "user_id": 94829,
      "name": "Anycubic Photon Mono M7 Pro",
      "nonce": "",
      "key": "0623a919ab9f14bfd7670e65f1449dff",
      "machine_type": 128,
      "model": "Anycubic Photon Mono M7 Pro",
      "img": "https://cdn.cloud-universe.anycubic.com/device/photon_mono_m7_pro.png",
      "description": "0142-A91C-40DF-168E",
      "type": "LCD",
      "device_status": 1,
      "ready_status": 0,
      "is_printing": 1,
      "reason": "free",
      "video_taskid": 0,
      "msg": "",
      "material_used": "565.56ml",
      "print_totaltime": "11hour23min",
      "status": 1,
      "machine_mac": "C0-09-25-1E-33-F6",
      "delete": 0,
      "create_time": 1772446717,
      "delete_time": 0,
      "last_update_time": 1772820234179,
      "label_name": "",
      "is_clean_plate": 0,
      "label_id": 0,
      "max_box_num": 0,
      "machine_data": {
        "name": "Anycubic Photon Mono M7 Pro",
        "pixel": 34.4,
        "res_x": 11520,
        "res_y": 5120,
        "format": "pw0Img",
        "size_x": 223.642,
        "size_y": 126.48,
        "size_z": 230.0,
        "suffix": "pwsz",
        "anti_max": 8
      },
      "type_function_ids": [
        1,
        2,
        3,
        26,
        27,
        29,
        31,
        33,
        34,
        35
      ],
      "material_type": "Resin",
      "version": {
        "need_update": 0,
        "firmware_version": "4.0.8.6"
      },
      "multi_color_box": [],
      "color": [],
      "features": null
    },
    {
      "id": 42859,
      "user_id": 94829,
      "name": "Anycubic Photon M3 Plus",
      "nonce": "",
      "key": "35b1681ce52f58f18feffd6880a43d36",
      "machine_type": 107,
      "model": "Anycubic Photon M3 Plus",
      "img": "https://cdn.cloud-platform.anycubic.com/php/img/4/m3plus.png",
      "description": "A7F6-B0FF-F706-3D49",
      "type": "LCD",
      "device_status": 2,
      "ready_status": 0,
      "is_printing": 1,
      "reason": "offline",
      "video_taskid": 0,
      "msg": "",
      "material_used": "23997.16ml",
      "print_totaltime": "674hour8min",
      "status": 1,
      "machine_mac": "",
      "delete": 0,
      "create_time": 1708449350,
      "delete_time": 0,
      "last_update_time": 1772372584556,
      "label_name": "",
      "is_clean_plate": 0,
      "label_id": 0,
      "max_box_num": 0,
      "machine_data": {
        "name": "Anycubic Photon M3 Plus",
        "pixel": 34.4,
        "res_x": 5760,
        "res_y": 3600,
        "format": "pw0Img",
        "size_x": 197.0,
        "size_y": 122.8,
        "size_z": 245.0,
        "suffix": "pwmb",
        "anti_max": 8
      },
      "type_function_ids": [
        7,
        22
      ],
      "material_type": "Resin",
      "version": null,
      "multi_color_box": [],
      "color": [],
      "features": null
    }
  ],
  "pageData": {
    "count": 2,
    "total": 2,
    "page": 1,
    "page_count": 150
  }
}
```

### 16. printer_info_v2 - OK
- Endpoint: `GET https://cloud-universe.anycubic.com/p/p/workbench/api/v2/printer/info?id=525668`
- Source: `Docs/end_points.md::8.3 Info imprimante v2, Docs/end_points_v2_verifie.md`
- HTTP status: `200`
- Business code: `1`
- Duree: `412 ms`
- Response JSON:
```json
{
  "code": 1,
  "msg": "连接成功 (Connection successful)",
  "data": {
    "base": {
      "print_count": 10,
      "print_totaltime": "11hour23min",
      "material_type": "Resin",
      "material_used": "565.56ml",
      "description": "0142-A91C-40DF-168E",
      "create_time": 1772446717,
      "firmware_version": "4.0.8.6",
      "machine_mac": "C0-09-25-1E-33-F6"
    },
    "name": "Anycubic Photon Mono M7 Pro",
    "id": 525668,
    "key": "0623a919ab9f14bfd7670e65f1449dff",
    "img": "https://cdn.cloud-universe.anycubic.com/device/photon_mono_m7_pro.png",
    "machine_type": 128,
    "device_status": 1,
    "is_printing": 1,
    "model": "Anycubic Photon Mono M7 Pro",
    "free_temp_limit": "{}",
    "temp_limit": "{}",
    "machine_data": {
      "name": "Anycubic Photon Mono M7 Pro",
      "pixel": 34.4,
      "res_x": 11520,
      "res_y": 5120,
      "format": "pw0Img",
      "size_x": 223.642,
      "size_y": 126.48,
      "size_z": 230,
      "suffix": "pwsz",
      "anti_max": 8
    },
    "rotate_deg": 0,
    "type_function_ids": [
      1,
      2,
      3,
      26,
      27,
      29,
      31,
      33,
      34,
      35
    ],
    "nozzle_diameter": null,
    "tools": [
      {
        "id": 140,
        "typd_id": 0,
        "model_id": 128,
        "type_function_id": 29,
        "parent_id": 0,
        "function_name": "料盒清理",
        "function_des": "清理料槽底部残留的模型",
        "control": 0,
        "param": [],
        "icon_url": "https://cdn.cloud-universe.anycubic.com/php/img/4/29.png",
        "function_type": 1,
        "status": 1,
        "show_place": 1
      },
      {
        "id": 141,
        "typd_id": 0,
        "model_id": 128,
        "type_function_id": 3,
        "parent_id": 0,
        "function_name": "曝光检测",
        "function_des": "检查曝光屏状态是否正常",
        "control": 0,
        "param": [],
        "icon_url": "https://cdn.cloud-universe.anycubic.com/php/img/4/3.png",
        "function_type": 1,
        "status": 1,
        "show_place": 1
      },
      {
        "id": 142,
        "typd_id": 0,
        "model_id": 128,
        "type_function_id": 1,
        "parent_id": 0,
        "function_name": "移动Z轴",
        "function_des": "检查Z轴电机是否正常",
        "control": 0,
        "param": [],
        "icon_url": "https://cdn.cloud-universe.anycubic.com/php/img/4/1.png",
        "function_type": 1,
        "status": 1,
        "show_place": 1
      },
      {
        "id": 143,
        "typd_id": 0,
        "model_id": 128,
        "type_function_id": 2,
        "parent_id": 0,
        "function_name": "文件管理",
        "function_des": "查看或删除打印机本地存储的文件",
        "control": 0,
        "param": [],
        "icon_url": "https://cdn.cloud-universe.anycubic.com/php/img/4/2.png",
        "function_type": 1,
        "status": 1,
        "show_place": 1
      },
      {
        "id": 147,
        "typd_id": 0,
        "model_id": 128,
        "type_function_id": 31,
        "parent_id": 0,
        "function_name": "离型膜状态",
        "function_des": "查看或重置离型膜的打印次数或层数",
        "control": 0,
        "param": [
          {
            "help_url": "https://wiki.anycubic.com/en/resin-3d-printer/Common/the-fep-film-is-damaged-or-the-end-of-expected-usage-life"
          }
        ],
        "icon_url": "https://cdn.cloud-universe.anycubic.com/php/img/4/31.png",
        "function_type": 1,
        "status": 1,
        "show_place": 1
      },
      {
        "id": 152,
        "typd_id": 0,
        "model_id": 128,
        "type_function_id": 33,
        "parent_id": 0,
        "function_name": "智能料盒",
        "function_des": "启动料盒循环清洗功能",
        "control": 1,
        "param": [
          {
            "title": "自动加热",
            "des": "设置打印前的自动加热温度",
            "title_value": "45",
            "title_value_type": 2,
            "name": "heating_temperature"
          },
          {
            "title": "循环清洗",
            "des": "点击之后开始对料槽进行循环清洗",
            "title_value": "",
            "title_value_type": 0,
            "name": "cycle_cleaning"
          }
        ],
        "icon_url": "https://cdn.cloud-universe.anycubic.com/device_function/smart_box.png",
        "function_type": 1,
        "status": 1,
        "show_place": 1
      },
      {
        "id": 153,
        "typd_id": 0,
        "model_id": 128,
        "type_function_id": 34,
        "parent_id": 0,
        "function_name": "自动进退料",
        "function_des": "自动添加、回收料盒里的树脂",
        "control": 1,
        "param": [
          {
            "title": "自动进料",
            "des": "点击立即开始，立即开始进行自动进料流程",
            "title_value": "",
            "title_value_type": 0,
            "name": "automatic_feeding"
          },
          {
            "title": "自动退料",
            "des": "点击立即开始，立即开始进行自动进料流程",
            "title_value": "",
            "title_value_type": 0,
            "name": "automatic_rewinding"
          }
        ],
        "icon_url": "https://cdn.cloud-universe.anycubic.com/device_function/auto_feed_rewrd.png",
        "function_type": 1,
        "status": 1,
        "show_place": 1
      }
    ],
    "advance": [
      {
        "id": 151,
        "typd_id": 0,
        "model_id": 128,
        "type_function_id": 35,
        "parent_id": 0,
        "function_name": "打印功能设置",
        "function_des": "当打印机检测到打印中有异常情况时，自动暂停或终止打印任务",
        "control": 1,
        "param": [
          {
            "name": "failDetection",
            "help_url": "https://cloud-platform.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=630",
            "title": "错误检测",
            "des": "",
            "support": 1,
            "editable": 1,
            "status": 1
          },
          {
            "name": "offLightCompensation",
            "help_url": "https://cloud-platform.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=630",
            "title": "灭灯补偿",
            "des": "",
            "support": 1,
            "editable": 1,
            "status": 1
          },
          {
            "name": "dynamicRelease",
            "help_url": "https://cloud-platform.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=630",
            "title": "智能离型",
            "des": "",
            "support": 1,
            "editable": 1,
            "status": 1
          },
          {
            "name": "resinAutoLoad",
            "help_url": "https://cloud-platform.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=630",
            "title": "自动进料",
            "des": "",
            "support": 1,
            "editable": 1,
            "status": 1
          },
          {
            "name": "resinHeat",
            "help_url": "https://cloud-platform.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=630",
            "title": "打印前加热",
            "des": "",
            "support": 1,
            "editable": 1,
            "status": 1
          },
          {
            "name": "autoTop",
            "help_url": "https://cloud-platform.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=630",
            "title": "打印完成抬升置顶",
            "des": "",
            "support": 1,
            "editable": 1,
            "status": 1
          },
          {
            "name": "resinCyclePrinting",
            "help_url": "https://cloud-platform.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=630",
            "title": "打印中循环",
            "des": "",
            "support": 0,
            "editable": 1,
            "status": 0
          },
          {
            "name": "levelling",
            "help_url": "https://cloud-platform.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=630",
            "title": "调平检测",
            "des": "检测打印平台是否调平失败",
            "support": 1,
            "editable": 0,
            "status": 1
          },
          {
            "name": "platform",
            "help_url": "https://cloud-platform.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=630",
            "title": "平台检测",
            "des": "检测打印平台是否已安装",
            "support": 1,
            "editable": 0,
            "status": 0
          },
          {
            "name": "resin",
            "help_url": "https://cloud-platform.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=630",
            "title": "树脂检测",
            "des": "树脂检测料槽中的树脂是否充足",
            "support": 1,
            "editable": 0,
            "status": 1
          },
          {
            "name": "residual",
            "help_url": "https://cloud-platform.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=630",
            "title": "残渣检测",
            "des": "检测料槽中是否存在模型残渣",
            "support": 1,
            "editable": 0,
            "status": 1
          },
          {
            "name": "resinAutoUnload",
            "help_url": "https://cloud-platform.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=630",
            "title": "自动退料",
            "des": "",
            "support": 0,
            "editable": 0,
            "status": 0
          },
          {
            "name": "resinCycle",
            "help_url": "https://cloud-platform.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=630",
            "title": "树脂循环",
            "des": "",
            "support": 0,
            "editable": 0,
            "status": 0
          }
        ],
        "icon_url": "https://cdn.cloud-universe.anycubic.com/php/img/4/26.png",
        "function_type": 2,
        "status": 1,
        "show_place": 1
      }
    ],
    "help_url": "https://wiki.anycubic.com/en/resin-3d-printer/photon-mono-m7-pro",
    "version": {
      "need_update": 0,
      "firmware_version": "4.0.8.6",
      "update_progress": 0,
      "update_date": 0,
      "update_status": "",
      "update_desc": "1、Upgraded automatic resin calibration.\n2、Optimized various user experiences. Please upgrade to the latest version",
      "force_update": 1,
      "target_version": "4.0.8.6",
      "time_cost": 100,
      "img": "https://cdn.cloud-universe.anycubic.com/device/photon_mono_m7_pro.png"
    },
    "quick_start_url": "https://wiki.anycubic.com/en/resin-3d-printer/photon-mono-m7-pro/-quick-start-guide-page-contents",
    "is_read_quick_start_url": 1,
    "maintenance_manual_url": "",
    "multi_color_box_version": null,
    "head_tools_model": 0,
    "external_shelves": {
      "id": 0,
      "type": "PLA",
      "color": [
        255,
        255,
        255
      ]
    },
    "need_update": 0,
    "releasefilm_url": "https://wiki.anycubic.com/en/resin-3d-printer/photon-mono-m7-pro",
    "multi_color_box": null,
    "releaseFilm": {
      "layers": 7159
    },
    "features": null,
    "max_box_num": 0
  }
}
```

### 17. printers_status_bulk - OK
- Endpoint: `GET https://cloud-universe.anycubic.com/p/p/workbench/api/v2/printer/printersStatus`
- Source: `Docs/end_points.md::8.4 Status imprimantes (bulk), Docs/end_points_v2_verifie.md`
- HTTP status: `200`
- Business code: `1`
- Duree: `368 ms`
- Response JSON:
```json
{
  "code": 1,
  "msg": "请求被接受 (Request accepted)",
  "data": [
    {
      "id": 525668,
      "name": "Anycubic Photon Mono M7 Pro",
      "key": "0623a919ab9f14bfd7670e65f1449dff",
      "machine_type": 128,
      "model": "Anycubic Photon Mono M7 Pro",
      "img": "https://cdn.cloud-universe.anycubic.com/device/photon_mono_m7_pro.png",
      "description": "0142-A91C-40DF-168E",
      "type": "LCD",
      "device_status": 1,
      "is_printing": 1,
      "available": 2,
      "reason": "unavailable reason:Slice file does not match machine type"
    },
    {
      "id": 42859,
      "name": "Anycubic Photon M3 Plus",
      "key": "35b1681ce52f58f18feffd6880a43d36",
      "machine_type": 107,
      "model": "Anycubic Photon M3 Plus",
      "img": "https://cdn.cloud-platform.anycubic.com/php/img/4/m3plus.png",
      "description": "A7F6-B0FF-F706-3D49",
      "type": "LCD",
      "device_status": 2,
      "is_printing": 1,
      "available": 2,
      "reason": "unavailable reason:Slice file does not match machine type"
    }
  ]
}
```

### 18. printers_status_by_ext_pwmb - OK
- Endpoint: `GET https://cloud-universe.anycubic.com/p/p/workbench/api/v2/printer/printersStatus?file_ext=pwmb`
- Source: `Docs/end_points.md::8.5 Compatibilite par extension (file_ext), Docs/end_points_v2_verifie.md`
- HTTP status: `200`
- Business code: `1`
- Duree: `368 ms`
- Response JSON:
```json
{
  "code": 1,
  "msg": "请求被接受 (Request accepted)",
  "data": [
    {
      "id": 525668,
      "name": "Anycubic Photon Mono M7 Pro",
      "key": "0623a919ab9f14bfd7670e65f1449dff",
      "machine_type": 128,
      "model": "Anycubic Photon Mono M7 Pro",
      "img": "https://cdn.cloud-universe.anycubic.com/device/photon_mono_m7_pro.png",
      "description": "0142-A91C-40DF-168E",
      "type": "LCD",
      "device_status": 1,
      "is_printing": 1,
      "available": 2,
      "reason": "unavailable reason:Slice file does not match machine type"
    },
    {
      "id": 42859,
      "name": "Anycubic Photon M3 Plus",
      "key": "35b1681ce52f58f18feffd6880a43d36",
      "machine_type": 107,
      "model": "Anycubic Photon M3 Plus",
      "img": "https://cdn.cloud-platform.anycubic.com/php/img/4/m3plus.png",
      "description": "A7F6-B0FF-F706-3D49",
      "type": "LCD",
      "device_status": 2,
      "is_printing": 1,
      "available": 2,
      "reason": "unavailable reason:printer offline"
    }
  ]
}
```

### 19. printers_status_by_file_id - OK
- Endpoint: `GET https://cloud-universe.anycubic.com/p/p/workbench/api/v2/printer/printersStatus?file_id=55240781`
- Source: `Docs/end_points.md::8.6 Compatibilite par identifiant (file_id), Docs/end_points_v2_verifie.md`
- HTTP status: `200`
- Business code: `1`
- Duree: `379 ms`
- Response JSON:
```json
{
  "code": 1,
  "msg": "请求被接受 (Request accepted)",
  "data": [
    {
      "id": 525668,
      "name": "Anycubic Photon Mono M7 Pro",
      "key": "0623a919ab9f14bfd7670e65f1449dff",
      "machine_type": 128,
      "model": "Anycubic Photon Mono M7 Pro",
      "img": "https://cdn.cloud-universe.anycubic.com/device/photon_mono_m7_pro.png",
      "description": "0142-A91C-40DF-168E",
      "type": "LCD",
      "device_status": 1,
      "is_printing": 1,
      "available": 2,
      "reason": "unavailable reason:Slice file does not match machine type"
    },
    {
      "id": 42859,
      "name": "Anycubic Photon M3 Plus",
      "key": "35b1681ce52f58f18feffd6880a43d36",
      "machine_type": 107,
      "model": "Anycubic Photon M3 Plus",
      "img": "https://cdn.cloud-platform.anycubic.com/php/img/4/m3plus.png",
      "description": "A7F6-B0FF-F706-3D49",
      "type": "LCD",
      "device_status": 2,
      "is_printing": 1,
      "available": 2,
      "reason": "unavailable reason:printer offline"
    }
  ]
}
```

### 20. projects_list - OK
- Endpoint: `GET https://cloud-universe.anycubic.com/p/p/workbench/api/work/project/getProjects?limit=10&page=1&printer_id=525668`
- Source: `Docs/end_points.md::7.2 Liste projets (jobs), Docs/end_points_v2_verifie.md`
- HTTP status: `200`
- Business code: `1`
- Duree: `569 ms`
- Response JSON:
```json
{
  "code": 1,
  "msg": "请求被接受 (Request accepted)",
  "data": [
    {
      "id": 77178613,
      "taskid": 77178613,
      "user_id": 94829,
      "printer_id": 525668,
      "gcode_id": 76407508,
      "model": 0,
      "img": "",
      "estimate": 8100,
      "remain_time": 0,
      "material": "23.882999",
      "material_type": 10,
      "pause": 0,
      "progress": 100,
      "connect_status": 0,
      "print_status": 2,
      "reason": 0,
      "slice_data": null,
      "slice_status": 0,
      "status": 0,
      "ischeck": 2,
      "project_type": 1,
      "printed": 1,
      "create_time": 1772811440,
      "start_time": 0,
      "end_time": 1772819732,
      "slice_start_time": 0,
      "slice_end_time": 0,
      "total_time": "2hour11min",
      "print_time": 131,
      "slice_param": "{\"taskid\":\"0\",\"task_mode\":1,\"localtask\":\"d565671f-da36-408d-a68e-b0f9d0cfb23d\",\"filename\":\"Beetle_fix2.pwsz\",\"remain_time\":135,\"model_hight\":76.700005,\"curr_layer\":0,\"total_layers\":1534,\"supplies_usage\":23.882999,\"progress\":0,\"z_thick\":0.05,\"anti_count\":8,\"print_time\":0,\"slicer\":\"UVtools\",\"settings\":{\"on_time\":1.5,\"off_time\":0.5,\"bottom_time\":20,\"bottom_layers\":4,\"z_up_height\":3,\"z_up_speed\":3,\"z_down_speed\":3},\"settings_adv\":{\"z_up_height_b0\":3,\"z_up_height_b1\":4,\"z_up_speed_b0\":1,\"z_up_speed_b1\":3,\"z_down_speed_b0\":1,\"z_down_speed_b1\":3,\"z_up_height_n0\":3,\"z_up_height_n1\":3,\"z_up_speed_n0\":3,\"z_up_speed_n1\":6,\"z_down_speed_n0\":3,\"z_down_speed_n1\":6,\"trans_layers\":6},\"bott_time\":20,\"bott_layers\":4,\"exposure_time\":1.5,\"off_time\":0.5,\"layers\":1534,\"zthick\":0.05,\"estimate\":8100,\"basic_control_param\":{\"zup_speed\":3,\"zdown_speed\":3,\"zup_height\":3},\"time\":1772811440}",
      "delete": 0,
      "auto_operation": "[{\"name\":\"autoTop\",\"status\":-2},{\"name\":\"dynamicRelease\",\"status\":-2},{\"name\":\"failDetection\",\"status\":-2},{\"name\":\"levelling\",\"status\":0},{\"name\":\"model\",\"status\":-2},{\"name\":\"offLightCompensation\",\"status\":-2},{\"name\":\"platform\",\"status\":0},{\"name\":\"residual\",\"status\":0},{\"name\":\"resin\",\"status\":0},{\"name\":\"resinAutoLoad\",\"status\":-2},{\"name\":\"resinAutoUnload\",\"status\":-2},{\"name\":\"resinCycle\",\"status\":-2},{\"name\":\"resinCyclePrinting\",\"status\":-2},{\"name\":\"resinHeat\",\"status\":-2}]",
      "monitor": "[{\"name\":\"airCleanerDev\",\"status\":-2},{\"name\":\"fpgaDev\",\"status\":-2},{\"name\":\"motor\",\"status\":-2},{\"name\":\"platformDev\",\"status\":-2},{\"name\":\"pullForce\",\"status\":0},{\"name\":\"resiBoxDev\",\"status\":-2},{\"name\":\"resinInOutDev\",\"status\":-2},{\"name\":\"screen0\",\"status\":-2},{\"name\":\"screen1\",\"status\":-2},{\"name\":\"spiFlashDev\",\"status\":-2},{\"name\":\"usbDev\",\"status\":-2},{\"name\":\"uvBoard\",\"status\":-2},{\"name\":\"wifiDev\",\"status\":0}]",
      "last_update_time": 1772819731395,
      "settings": "{\"taskid\":77178613,\"task_mode\":1,\"localtask\":\"d565671f-da36-408d-a68e-b0f9d0cfb23d\",\"filename\":\"Beetle_fix2.pwsz\",\"remain_time\":0,\"model_hight\":76.700005,\"curr_layer\":1534,\"total_layers\":1534,\"supplies_usage\":23.882999,\"progress\":100,\"z_thick\":0.05,\"anti_count\":8,\"print_time\":131,\"slicer\":\"UVtools\",\"settings\":{\"on_time\":1.5,\"off_time\":0.5,\"bottom_time\":20,\"bottom_layers\":4,\"z_up_height\":3,\"z_up_speed\":3,\"z_down_speed\":3},\"settings_adv\":{\"z_up_height_b0\":3,\"z_up_height_b1\":4,\"z_up_speed_b0\":1,\"z_up_speed_b1\":3,\"z_down_speed_b0\":1,\"z_down_speed_b1\":3,\"z_up_height_n0\":3,\"z_up_height_n1\":3,\"z_up_speed_n0\":3,\"z_up_speed_n1\":6,\"z_down_speed_n0\":3,\"z_down_speed_n1\":6,\"trans_layers\":6},\"timestamp\":1772819731395,\"state\":\"finished\",\"reason\":200,\"action\":\"start\",\"err_message\":\"\"}",
      "localtask": "d565671f-da36-408d-a68e-b0f9d0cfb23d",
      "source": "local",
      "device_message": "{\"taskid\":\"77178613\",\"task_mode\":1,\"localtask\":\"d565671f-da36-408d-a68e-b0f9d0cfb23d\",\"filename\":\"Beetle_fix2.pwsz\",\"remain_time\":0,\"model_hight\":76.700005,\"curr_layer\":1534,\"total_layers\":1534,\"supplies_usage\":23.882999,\"progress\":100,\"z_thick\":0.05,\"anti_count\":8,\"print_time\":131,\"slicer\":\"UVtools\",\"settings\":{\"on_time\":1.5,\"off_time\":0.5,\"bottom_time\":20,\"bottom_layers\":4,\"z_up_height\":3,\"z_up_speed\":3,\"z_down_speed\":3},\"settings_adv\":{\"z_up_height_b0\":3,\"z_up_height_b1\":4,\"z_up_speed_b0\":1,\"z_up_speed_b1\":3,\"z_down_speed_b0\":1,\"z_down_speed_b1\":3,\"z_up_height_n0\":3,\"z_up_height_n1\":3,\"z_up_speed_n0\":3,\"z_up_speed_n1\":6,\"z_down_speed_n0\":3,\"z_down_speed_n1\":6,\"trans_layers\":6}}",
      "signal_strength": 54,
      "is_comment": 0,
      "is_makeronline_file": 0,
      "is_web_evoke": 0,
      "evoke_from": 0,
      "post_title": null,
      "key": "0623a919ab9f14bfd7670e65f1449dff",
      "type": "LCD",
      "machine_type": 128,
      "printer_name": "Anycubic Photon Mono M7 Pro",
      "machine_name": "Anycubic Photon Mono M7 Pro",
      "device_status": 1,
      "machine_class": 0,
      "gcode_name": "Beetle_fix2",
      "image_id": "",
      "slice_result": "{\"taskid\":\"0\",\"task_mode\":1,\"localtask\":\"d565671f-da36-408d-a68e-b0f9d0cfb23d\",\"filename\":\"Beetle_fix2.pwsz\",\"remain_time\":135,\"model_hight\":76.700005,\"curr_layer\":0,\"total_layers\":1534,\"supplies_usage\":23.882999,\"progress\":0,\"z_thick\":0.05,\"anti_count\":8,\"print_time\":0,\"slicer\":\"UVtools\",\"settings\":{\"on_time\":1.5,\"off_time\":0.5,\"bottom_time\":20,\"bottom_layers\":4,\"z_up_height\":3,\"z_up_speed\":3,\"z_down_speed\":3},\"settings_adv\":{\"z_up_height_b0\":3,\"z_up_height_b1\":4,\"z_up_speed_b0\":1,\"z_up_speed_b1\":3,\"z_down_speed_b0\":1,\"z_down_speed_b1\":3,\"z_up_height_n0\":3,\"z_up_height_n1\":3,\"z_up_speed_n0\":3,\"z_up_speed_n1\":6,\"z_down_speed_n0\":3,\"z_down_speed_n1\":6,\"trans_layers\":6},\"bott_time\":20,\"bott_layers\":4,\"exposure_time\":1.5,\"off_time\":0.5,\"layers\":1534,\"zthick\":0.05,\"estimate\":8100,\"basic_control_param\":{\"zup_speed\":3,\"zdown_speed\":3,\"zup_height\":3},\"time\":1772811440}",
      "dual_platform_mode_enable": 0
    },
    {
      "id": 77024367,
      "taskid": 77024367,
      "user_id": 94829,
      "printer_id": 525668,
      "gcode_id": 76253647,
      "model": 0,
      "img": "",
      "estimate": 8100,
      "remain_time": 0,
      "material": "23.882999",
      "material_type": 10,
      "pause": 0,
      "progress": 100,
      "connect_status": 0,
      "print_status": 2,
      "reason": 0,
      "slice_data": null,
      "slice_status": 0,
      "status": 0,
      "ischeck": 2,
      "project_type": 1,
      "printed": 1,
      "create_time": 1772737217,
      "start_time": 0,
      "end_time": 1772745488,
      "slice_start_time": 0,
      "slice_end_time": 0,
      "total_time": "2hour11min",
      "print_time": 131,
      "slice_param": "{\"taskid\":\"0\",\"task_mode\":1,\"localtask\":\"5c81f985-8ec6-487f-92e4-eae0c2533f8f\",\"filename\":\"Beetle_fix2.pwsz\",\"remain_time\":135,\"model_hight\":76.700005,\"curr_layer\":0,\"total_layers\":1534,\"supplies_usage\":23.882999,\"progress\":0,\"z_thick\":0.05,\"anti_count\":8,\"print_time\":0,\"slicer\":\"UVtools\",\"settings\":{\"on_time\":1.5,\"off_time\":0.5,\"bottom_time\":20,\"bottom_layers\":4,\"z_up_height\":3,\"z_up_speed\":3,\"z_down_speed\":3},\"settings_adv\":{\"z_up_height_b0\":3,\"z_up_height_b1\":4,\"z_up_speed_b0\":1,\"z_up_speed_b1\":3,\"z_down_speed_b0\":1,\"z_down_speed_b1\":3,\"z_up_height_n0\":3,\"z_up_height_n1\":3,\"z_up_speed_n0\":3,\"z_up_speed_n1\":6,\"z_down_speed_n0\":3,\"z_down_speed_n1\":6,\"trans_layers\":6},\"bott_time\":20,\"bott_layers\":4,\"exposure_time\":1.5,\"off_time\":0.5,\"layers\":1534,\"zthick\":0.05,\"estimate\":8100,\"basic_control_param\":{\"zup_speed\":3,\"zdown_speed\":3,\"zup_height\":3},\"time\":1772737217}",
      "delete": 0,
      "auto_operation": "[{\"name\":\"autoTop\",\"status\":-2},{\"name\":\"dynamicRelease\",\"status\":-2},{\"name\":\"failDetection\",\"status\":-2},{\"name\":\"levelling\",\"status\":0},{\"name\":\"model\",\"status\":-2},{\"name\":\"offLightCompensation\",\"status\":-2},{\"name\":\"platform\",\"status\":0},{\"name\":\"residual\",\"status\":0},{\"name\":\"resin\",\"status\":0},{\"name\":\"resinAutoLoad\",\"status\":-2},{\"name\":\"resinAutoUnload\",\"status\":-2},{\"name\":\"resinCycle\",\"status\":-2},{\"name\":\"resinCyclePrinting\",\"status\":-2},{\"name\":\"resinHeat\",\"status\":-2}]",
      "monitor": "[{\"name\":\"airCleanerDev\",\"status\":-2},{\"name\":\"fpgaDev\",\"status\":-2},{\"name\":\"motor\",\"status\":-2},{\"name\":\"platformDev\",\"status\":-2},{\"name\":\"pullForce\",\"status\":0},{\"name\":\"resiBoxDev\",\"status\":-2},{\"name\":\"resinInOutDev\",\"status\":-2},{\"name\":\"screen0\",\"status\":-2},{\"name\":\"screen1\",\"status\":-2},{\"name\":\"spiFlashDev\",\"status\":-2},{\"name\":\"usbDev\",\"status\":-2},{\"name\":\"uvBoard\",\"status\":-2},{\"name\":\"wifiDev\",\"status\":0}]",
      "last_update_time": 1772745487765,
      "settings": "{\"taskid\":77024367,\"task_mode\":1,\"localtask\":\"5c81f985-8ec6-487f-92e4-eae0c2533f8f\",\"filename\":\"Beetle_fix2.pwsz\",\"remain_time\":0,\"model_hight\":76.700005,\"curr_layer\":1534,\"total_layers\":1534,\"supplies_usage\":23.882999,\"progress\":100,\"z_thick\":0.05,\"anti_count\":8,\"print_time\":131,\"slicer\":\"UVtools\",\"settings\":{\"on_time\":1.5,\"off_time\":0.5,\"bottom_time\":20,\"bottom_layers\":4,\"z_up_height\":3,\"z_up_speed\":3,\"z_down_speed\":3},\"settings_adv\":{\"z_up_height_b0\":3,\"z_up_height_b1\":4,\"z_up_speed_b0\":1,\"z_up_speed_b1\":3,\"z_down_speed_b0\":1,\"z_down_speed_b1\":3,\"z_up_height_n0\":3,\"z_up_height_n1\":3,\"z_up_speed_n0\":3,\"z_up_speed_n1\":6,\"z_down_speed_n0\":3,\"z_down_speed_n1\":6,\"trans_layers\":6},\"timestamp\":1772745487765,\"state\":\"finished\",\"reason\":200,\"action\":\"start\",\"err_message\":\"\"}",
      "localtask": "5c81f985-8ec6-487f-92e4-eae0c2533f8f",
      "source": "local",
      "device_message": "{\"taskid\":\"77024367\",\"task_mode\":1,\"localtask\":\"5c81f985-8ec6-487f-92e4-eae0c2533f8f\",\"filename\":\"Beetle_fix2.pwsz\",\"remain_time\":0,\"model_hight\":76.700005,\"curr_layer\":1534,\"total_layers\":1534,\"supplies_usage\":23.882999,\"progress\":100,\"z_thick\":0.05,\"anti_count\":8,\"print_time\":131,\"slicer\":\"UVtools\",\"settings\":{\"on_time\":1.5,\"off_time\":0.5,\"bottom_time\":20,\"bottom_layers\":4,\"z_up_height\":3,\"z_up_speed\":3,\"z_down_speed\":3},\"settings_adv\":{\"z_up_height_b0\":3,\"z_up_height_b1\":4,\"z_up_speed_b0\":1,\"z_up_speed_b1\":3,\"z_down_speed_b0\":1,\"z_down_speed_b1\":3,\"z_up_height_n0\":3,\"z_up_height_n1\":3,\"z_up_speed_n0\":3,\"z_up_speed_n1\":6,\"z_down_speed_n0\":3,\"z_down_speed_n1\":6,\"trans_layers\":6}}",
      "signal_strength": 52,
      "is_comment": 0,
      "is_makeronline_file": 0,
      "is_web_evoke": 0,
      "evoke_from": 0,
      "post_title": null,
      "key": "0623a919ab9f14bfd7670e65f1449dff",
      "type": "LCD",
      "machine_type": 128,
      "printer_name": "Anycubic Photon Mono M7 Pro",
      "machine_name": "Anycubic Photon Mono M7 Pro",
      "device_status": 1,
      "machine_class": 0,
      "gcode_name": "Beetle_fix2",
      "image_id": "",
      "slice_result": "{\"taskid\":\"0\",\"task_mode\":1,\"localtask\":\"5c81f985-8ec6-487f-92e4-eae0c2533f8f\",\"filename\":\"Beetle_fix2.pwsz\",\"remain_time\":135,\"model_hight\":76.700005,\"curr_layer\":0,\"total_layers\":1534,\"supplies_usage\":23.882999,\"progress\":0,\"z_thick\":0.05,\"anti_count\":8,\"print_time\":0,\"slicer\":\"UVtools\",\"settings\":{\"on_time\":1.5,\"off_time\":0.5,\"bottom_time\":20,\"bottom_layers\":4,\"z_up_height\":3,\"z_up_speed\":3,\"z_down_speed\":3},\"settings_adv\":{\"z_up_height_b0\":3,\"z_up_height_b1\":4,\"z_up_speed_b0\":1,\"z_up_spe
... <truncated 47244 chars>
```

### 21. project_info_v2 - OK
- Endpoint: `GET https://cloud-universe.anycubic.com/p/p/workbench/api/v2/project/info?id=77178613`
- Source: `Docs/end_points.md::7.3 Detail projet, Docs/end_points_v2_verifie.md`
- HTTP status: `200`
- Business code: `1`
- Duree: `368 ms`
- Response JSON:
```json
{
  "code": 1,
  "msg": "连接成功 (Connection successful)",
  "data": {
    "gcode_name": "Beetle_fix2",
    "post_id": 0,
    "post_title": null,
    "printer_name": "Anycubic Photon Mono M7 Pro",
    "printer_type": "LCD",
    "key": "0623a919ab9f14bfd7670e65f1449dff",
    "machine_type": 128,
    "machine_name": "Anycubic Photon Mono M7 Pro",
    "model": "Anycubic Photon Mono M7 Pro",
    "img": "",
    "progress": 100,
    "print_status": 2,
    "reason": "0",
    "reason_id": 0,
    "slice_status": 0,
    "project_type": 1,
    "pause": 0,
    "material": "23.882999",
    "create_time": 1772811440,
    "end_time": 1772819732,
    "gcode_id": 76407508,
    "type_function_ids": [
      1,
      2,
      3,
      26,
      27,
      29,
      31,
      33,
      34,
      35
    ],
    "id": 77178613,
    "total_time": "138.2",
    "device_message": {
      "taskid": "77178613",
      "task_mode": 1,
      "localtask": "d565671f-da36-408d-a68e-b0f9d0cfb23d",
      "filename": "Beetle_fix2.pwsz",
      "remain_time": 0,
      "model_hight": 76.700005,
      "curr_layer": 1534,
      "total_layers": 1534,
      "supplies_usage": 23.882999,
      "progress": 100,
      "z_thick": 0.05,
      "anti_count": 8,
      "print_time": 131,
      "slicer": "UVtools",
      "settings": {
        "on_time": 1.5,
        "off_time": 0.5,
        "bottom_time": 20,
        "bottom_layers": 4,
        "z_up_height": 3,
        "z_up_speed": 3,
        "z_down_speed": 3
      },
      "settings_adv": {
        "z_up_height_b0": 3,
        "z_up_height_b1": 4,
        "z_up_speed_b0": 1,
        "z_up_speed_b1": 3,
        "z_down_speed_b0": 1,
        "z_down_speed_b1": 3,
        "z_up_height_n0": 3,
        "z_up_height_n1": 3,
        "z_up_speed_n0": 3,
        "z_up_speed_n1": 6,
        "z_down_speed_n0": 3,
        "z_down_speed_n1": 6,
        "trans_layers": 6
      },
      "timestamp": 1772819731395,
      "state": "finished",
      "reason": 200,
      "action": "start",
      "err_message": "",
      "heating_remain_time": 0
    },
    "task_mode": 1,
    "reason_help_url": null,
    "sliced_plates": null,
    "z_thick": 0.05,
    "rotate_deg": 0,
    "task_settings": {},
    "slice_param": {
      "zthick": 0.05,
      "bott_layers": 4,
      "bott_time": 20,
      "exposure_time": 1.5,
      "off_time": 0.5,
      "zup_speed": 3,
      "zup_height": 3,
      "zdown_speed": 3,
      "material_name": "Resin"
    },
    "material_unit": "ml",
    "slice_result": {
      "size_x": 0,
      "size_y": 0,
      "size_z": 0
    },
    "set_limit": {
      "bott_layers": [
        1,
        200
      ],
      "bott_time": [
        0.1,
        200
      ],
      "exposure_time": [
        0.1,
        200
      ],
      "off_time": [
        0.1,
        200
      ],
      "zthick": [
        0.01,
        0.3
      ],
      "zdown_speed": [
        0.1,
        20
      ],
      "zup_height": [
        0,
        50
      ],
      "zup_speed": [
        0,
        50
      ],
      "auto_support": {
        "lift_height": [
          0.0,
          1000.0
        ],
        "angle": [
          0,
          80
        ],
        "density": [
          0,
          99
        ],
        "min_length": [
          0,
          20
        ]
      }
    },
    "auto_operation": [
      {
        "name": "autoTop",
        "status": -2
      },
      {
        "name": "dynamicRelease",
        "status": -2
      },
      {
        "name": "failDetection",
        "status": -2
      },
      {
        "name": "levelling",
        "status": 0
      },
      {
        "name": "model",
        "status": -2
      },
      {
        "name": "offLightCompensation",
        "status": -2
      },
      {
        "name": "platform",
        "status": 0
      },
      {
        "name": "residual",
        "status": 0
      },
      {
        "name": "resin",
        "status": 0
      },
      {
        "name": "resinAutoLoad",
        "status": -2
      },
      {
        "name": "resinAutoUnload",
        "status": -2
      },
      {
        "name": "resinCycle",
        "status": -2
      },
      {
        "name": "resinCyclePrinting",
        "status": -2
      },
      {
        "name": "resinHeat",
        "status": -2
      }
    ],
    "monitor": [
      {
        "name": "airCleanerDev",
        "status": -2
      },
      {
        "name": "fpgaDev",
        "status": -2
      },
      {
        "name": "motor",
        "status": -2
      },
      {
        "name": "platformDev",
        "status": -2
      },
      {
        "name": "pullForce",
        "status": 0
      },
      {
        "name": "resiBoxDev",
        "status": -2
      },
      {
        "name": "resinInOutDev",
        "status": -2
      },
      {
        "name": "screen0",
        "status": -2
      },
      {
        "name": "screen1",
        "status": -2
      },
      {
        "name": "spiFlashDev",
        "status": -2
      },
      {
        "name": "usbDev",
        "status": -2
      },
      {
        "name": "uvBoard",
        "status": -2
      },
      {
        "name": "wifiDev",
        "status": 0
      }
    ],
    "printer_monitor": 1,
    "is_feedback": 0
  }
}
```

### 22. project_report - OK
- Endpoint: `GET https://cloud-universe.anycubic.com/p/p/workbench/api/work/project/report?id=77178613`
- Source: `Docs/end_points.md::7.3 Detail projet, Docs/end_points_v2_verifie.md`
- HTTP status: `200`
- Business code: `1`
- Duree: `322 ms`
- Response JSON:
```json
{
  "code": 1,
  "msg": "连接成功 (Connection successful)",
  "data": {
    "create_time": 1772811440,
    "end_time": 1772819732,
    "estimate": 8100,
    "gcode_name": "Beetle_fix2",
    "id": 77178613,
    "img": "",
    "is_comment": 0,
    "is_feedback": 0,
    "key": "0623a919ab9f14bfd7670e65f1449dff",
    "m_file": null,
    "machine_name": "Anycubic Photon Mono M7 Pro",
    "machine_type": 128,
    "material_unit": "ml",
    "post_id": 0,
    "post_title": null,
    "print_size": "-",
    "print_status": 2,
    "progress": 100,
    "project_type": 1,
    "reason": "Unknown reason",
    "slice_param": {
      "bott_layers": 4,
      "bott_time": 20,
      "exposure_time": 1.5,
      "material_name": "树脂",
      "off_time": 0.5,
      "zdown_speed": 3,
      "zthick": 0.05,
      "zup_height": 3,
      "zup_speed": 3
    },
    "slice_status": 0,
    "sliced_plates": null,
    "supplies_usage": 23.882999,
    "total_layers": 1534,
    "type": "LCD"
  }
}
```

### 23. send_print_order - SKIPPED
- Endpoint: `POST https://cloud-universe.anycubic.com/p/p/workbench/api/work/operation/sendOrder`
- Source: `Docs/end_points.md::7.1 Send print order, Docs/end_points_v2_verifie.md`
- Raison du skip: Desactive par defaut (action intrusive). Utiliser --allow-send-order.

### 24. message_count - OK
- Endpoint: `GET https://cloud-universe.anycubic.com/p/p/workbench/api/v2/message/getMessageCount`
- Source: `Docs/end_points_v2_verifie.md::2) Endpoints presents dans end_points.md mais non utilises directement par la v2 Python, Docs/end_points.md`
- HTTP status: `200`
- Business code: `1`
- Duree: `362 ms`
- Response JSON:
```json
{
  "code": 1,
  "msg": "请求被接受 (Request accepted)",
  "data": [
    {
      "count": 0,
      "newcount": 0,
      "type": 3,
      "create_time": 0,
      "title": "",
      "content": "No new information"
    },
    {
      "count": 72,
      "newcount": 69,
      "create_time": 1772819732,
      "title": "Print complete",
      "content": "\"Beetle_fix2\" Printers have been completed.",
      "type": 1
    },
    {
      "count": 0,
      "newcount": 0,
      "type": 5,
      "create_time": 0,
      "title": "",
      "content": "No new information"
    }
  ]
}
```

### 25. project_print_history - OK
- Endpoint: `GET https://cloud-universe.anycubic.com/p/p/workbench/api/v2/project/printHistory?page=1&limit=10`
- Source: `Docs/end_points_v2_verifie.md::2) Endpoints presents dans end_points.md mais non utilises directement par la v2 Python, Docs/end_points.md`
- HTTP status: `200`
- Business code: `1`
- Duree: `311 ms`
- Response JSON:
```json
{
  "code": 1,
  "msg": "请求被接受 (Request accepted)",
  "data": [
    {
      "create_time": 1772811440,
      "end_time": 1772819732,
      "gcode_name": "Beetle_fix2",
      "img": "",
      "machine_type": 128,
      "print_status": 2,
      "printer_id": 525668,
      "printer_name": "Anycubic Photon Mono M7 Pro",
      "task_id": 77178613
    },
    {
      "create_time": 1772737217,
      "end_time": 1772745488,
      "gcode_name": "Beetle_fix2",
      "img": "",
      "machine_type": 128,
      "print_status": 2,
      "printer_id": 525668,
      "printer_name": "Anycubic Photon Mono M7 Pro",
      "task_id": 77024367
    },
    {
      "create_time": 1772726035,
      "end_time": 1772734660,
      "gcode_name": "Beetle_fix2",
      "img": "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026-03/05/jpg/e7b02b598f6246689c4a4c0af683fc42.jpg",
      "machine_type": 128,
      "print_status": 2,
      "printer_id": 525668,
      "printer_name": "Anycubic Photon Mono M7 Pro",
      "task_id": 76984784
    },
    {
      "create_time": 1772705961,
      "end_time": 1772709174,
      "gcode_name": "01_T3d_skull_cut_v3",
      "img": "",
      "machine_type": 128,
      "print_status": 2,
      "printer_id": 525668,
      "printer_name": "Anycubic Photon Mono M7 Pro",
      "task_id": 76931010
    },
    {
      "create_time": 1772661047,
      "end_time": 1772664332,
      "gcode_name": "01_T3d_skull_cut_v3",
      "img": "",
      "machine_type": 128,
      "print_status": 2,
      "printer_id": 525668,
      "printer_name": "Anycubic Photon Mono M7 Pro",
      "task_id": 76855297
    },
    {
      "create_time": 1772633321,
      "end_time": 1772635758,
      "gcode_name": "01_T3d_skull_cut_v3",
      "img": "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026-03/04/jpg/042f0e8fdbac4358a362be22582cbef5.jpg",
      "machine_type": 128,
      "print_status": 2,
      "printer_id": 525668,
      "printer_name": "Anycubic Photon Mono M7 Pro",
      "task_id": 76767166
    },
    {
      "create_time": 1772625084,
      "end_time": 1772628079,
      "gcode_name": "01_T3d_skull_cut_v4",
      "img": "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026-03/03/jpg/24d48d956e5343b98c97755a5fca53e8.jpg",
      "machine_type": 128,
      "print_status": 2,
      "printer_id": 525668,
      "printer_name": "Anycubic Photon Mono M7 Pro",
      "task_id": 76745406
    },
    {
      "create_time": 1772554183,
      "end_time": 1772557512,
      "gcode_name": "skull-14-24-v4",
      "img": "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026-03/03/jpg/33adaa32999b42d1a6268966b117ca5a.jpg",
      "machine_type": 128,
      "print_status": 2,
      "printer_id": 525668,
      "printer_name": "Anycubic Photon Mono M7 Pro",
      "task_id": 76592271
    },
    {
      "create_time": 1772547377,
      "end_time": 1772551028,
      "gcode_name": "skull-14-24-v3",
      "img": "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026-03/03/jpg/876a7b29d7cc4fa69b9dac60f862294c.jpg",
      "machine_type": 128,
      "print_status": 2,
      "printer_id": 525668,
      "printer_name": "Anycubic Photon Mono M7 Pro",
      "task_id": 76571009
    },
    {
      "create_time": 1772463501,
      "end_time": 1772465498,
      "gcode_name": "01_Skull_8",
      "img": "",
      "machine_type": 128,
      "print_status": 2,
      "printer_id": 525668,
      "printer_name": "Anycubic Photon Mono M7 Pro",
      "task_id": 76381433
    }
  ],
  "pageData": {
    "page": 1,
    "page_count": 10,
    "total": 98
  }
}
```

### 26. reason_catalog - OK
- Endpoint: `GET https://cloud-universe.anycubic.com/p/p/workbench/api/portal/index/reason`
- Source: `Docs/end_points.md::11.1 Reasons catalog, Docs/end_points_v2_verifie.md`
- HTTP status: `200`
- Business code: `1`
- Duree: `545 ms`
- Response JSON:
```json
{
  "code": 1,
  "msg": "请求被接受 (Request accepted)",
  "data": [
    {
      "id": 1002,
      "reason": 101,
      "desc": "已存在打印任务",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1003,
      "reason": 102,
      "desc": "不存在打印任务",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1004,
      "reason": 103,
      "desc": "打印任务已暂停",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1005,
      "reason": 104,
      "desc": "打印任务已终止",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1006,
      "reason": 105,
      "desc": "打印机下载打印文件失败",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1007,
      "reason": 106,
      "desc": "3D打印机磁盘已满，请在打印机的文件列表中，删除历史文件后重试",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1008,
      "reason": 107,
      "desc": "挂载外部存储失败",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1009,
      "reason": 110,
      "desc": "切片文件错误，建议重新切片",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1010,
      "reason": 111,
      "desc": "任务异常结束",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1011,
      "reason": 112,
      "desc": "参数设定失败",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1012,
      "reason": 113,
      "desc": "切片文件错误，建议重新切片",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1013,
      "reason": 115,
      "desc": "打印机无法解析切片文件",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1014,
      "reason": 117,
      "desc": "设备发生异常，打印任务被终止",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1015,
      "reason": 118,
      "desc": "切片文件高度超过机器限制，无法启动打印任务",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1016,
      "reason": 119,
      "desc": "切片文件错误，建议重新切片",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1017,
      "reason": 120,
      "desc": "切片文件层厚小于0.01mm，大于2mm，文件解析失败，无法启动打印任务",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1018,
      "reason": 121,
      "desc": "切片文件大小为0，文件解析失败，无法启动打印任务",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1019,
      "reason": 122,
      "desc": "切片文件损坏，文件解析失败，无法启动打印任务",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1020,
      "reason": 123,
      "desc": "切片文件扩展名不正确，无法启动打印任务",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1021,
      "reason": 202,
      "desc": "切片文件错误，建议重新切片",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1022,
      "reason": 203,
      "desc": "切片文件错误，建议重新切片",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1023,
      "reason": 204,
      "desc": "切片文件错误，建议重新切片",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1024,
      "reason": 301,
      "desc": "设备忙碌，情况未知",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1025,
      "reason": 302,
      "desc": "打印机正在调试中",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1026,
      "reason": 401,
      "desc": "打印任务被暂停",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1027,
      "reason": 402,
      "desc": "树脂不足，暂停打印",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1028,
      "reason": 501,
      "desc": "打印任务被继续",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1029,
      "reason": 502,
      "desc": "树脂已补充，恢复打印",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1030,
      "reason": 601,
      "desc": "手动终止",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1031,
      "reason": 602,
      "desc": "断电续打终止",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1032,
      "reason": 603,
      "desc": "打印任务意外中断",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1033,
      "reason": 1001,
      "desc": "文件删除失败",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1100,
      "reason": 108,
      "desc": "打印文件的名称太长",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1101,
      "reason": 701,
      "desc": "文件下载失败",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1102,
      "reason": 702,
      "desc": "OTA更新失败",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1103,
      "reason": 703,
      "desc": "打印机本地空间不足",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1104,
      "reason": 704,
      "desc": "挂载外部存储失败",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1105,
      "reason": 705,
      "desc": "文件校验失败",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1106,
      "reason": 801,
      "desc": "打印中禁止操作",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1107,
      "reason": 802,
      "desc": "未复位设置零点失败",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1108,
      "reason": 803,
      "desc": "设置零点不在范围",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1109,
      "reason": 804,
      "desc": "曝光中禁止操作",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1110,
      "reason": 805,
      "desc": "复位中禁止操作",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1111,
      "reason": 806,
      "desc": "已到达高度限制，无法继续移动，请点击归零",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud-universe.anycubic.com/w/p/helpCenter/#/pages/pageDetail/pageDetails?id=547"
    },
    {
      "id": 1112,
      "reason": 901,
      "desc": "打印中禁止操作",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "https://cloud
... <truncated 68290 chars>
```
