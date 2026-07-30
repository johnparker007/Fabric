# Repository rules

- Do not commit generated build outputs, proprietary DLLs, or ROM images.
- Maintain the versioned public C ABI under `include/fabric/` carefully; do not change established
  layouts or symbols without an explicit ABI plan and contract tests.
- Catch and contain every exception within the `FabricRuntime` DLL boundary.
- Keep declarations of external production ABIs private to their backend implementation.
- Do not add emulator-core source to this repository.
- Add focused tests for every runtime or backend contract change.
- Keep behavioural changes separate from broad refactoring and formatting churn.
