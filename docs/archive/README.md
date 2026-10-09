# docs/archive/

**这个目录里的东西已经过时，不要再改，也不要引用。**

## 这里有什么

| 文件 | 大小 | 内容冻结于 | 说明 |
|---|---|---|---|
| `代码规范.docx` | 18,447 B | 2026-05-25 | 更早的 **Word 手写版**《代码规范》 |
| `项目计划书.docx` | 23,705 B | 2026-06-01 | 更早的 **Word 手写版**《项目计划书》 |

> 上面的字节数是**脱敏后**的大小；原始字节数是 17,546 B 与 20,970 B。差别来自归档入库时对成员真名做的替换，见下面「归档时对内容做过的改动」。

## 为什么它们在这里

**它们不是 `docs/markdown/` 里那些 `.md` 的导出产物，而是更早的 Word 手写稿。** 时间线是倒的：

- `代码规范.docx` 的最后一次内容改动是 `1b1f634`（2026-05-25），而 `docs/markdown/代码规范.md` **2026-06-01 才被创建**（`4f63c65`）；
- `项目计划书.docx` 的最后一次内容改动是 `4f63c65`（2026-06-01），与对应 md 同日，但 md 首版已经是带 Markdown 表格的重写稿，而 docx 的 `word/document.xml` 里 `w:tbl` 计数为 **0**（纯文本行，没有表格）。

docx 内部的 `docProps/core.xml` 与 git 提交日期交叉印证：

```xml
<dc:creator>xiaofusu123</dc:creator>
<cp:lastModifiedBy>xiaofusu123</cp:lastModifiedBy>
<cp:revision>21</cp:revision>          <!-- 代码规范.docx -->
<dcterms:created>2026-04-29T11:37:00Z</dcterms:created>
<dcterms:modified>2026-05-25T14:07:00Z</dcterms:modified>
```

```xml
<cp:revision>31</cp:revision>          <!-- 项目计划书.docx -->
<dcterms:created>2026-04-29T12:18:00Z</dcterms:created>
<dcterms:modified>2026-06-01T15:32:00Z</dcterms:modified>
```

另外还带一堆 Word 独有的指纹（`w:rsidRoot` / `w:proofState` / 中文版 Office 的 `w:themeFontLang zh-CN` / 两份 `word/styles.xml` 同为 41,941 B），**任何 md 渲染器（pandoc 等）都不会生成这些**。所以方向是 **docx → md**（手写稿被重写成 Markdown），不是 md → docx。

## 为什么不直接删掉

它们带 **md 无法表达的封面页与中文 `Normal.dotm` 样式**，可能是课程/答辩的 Word 交付材料。删除是破坏性的、且没有替代物，所以选择归档而不是删除。

## 归档时对内容做过的改动：真名脱敏

仓库是 **PUBLIC**，而这两份 Word 手写稿里写的是成员真名。归档入库时统一替换为对应的 GitHub 登录名，只动了两处 XML 文本：

| 位置 | 改动 |
|---|---|
| `word/document.xml`（仅 `项目计划书.docx`） | 分工表里的 5 个真名 → `xiaofusu123` / `RickeyDeung` / `ambulance001` / `Rikka2-aa` / `kuailede110`，共 17 处 |
| `docProps/core.xml`（两份都有） | `<dc:creator>` 与 `<cp:lastModifiedBy>` → `xiaofusu123`，共 4 处 |

除文本外**什么都没改**：两份文件仍是 12 个 zip entry，`word/styles.xml`、封面页、版式全部原样，Word 能正常打开。**原始未脱敏的字节仍留在 git 历史里**（`9a21f8a` 及其之前）——脱敏只保证 `HEAD` 不含真名；要连历史一起清掉必须重写历史，代价过大，不做。

## 它们里面有什么是 md 没有的

在归档之前已经把独有内容**全部并入**对应 md 了（`项目计划书.md` 6 条、`代码规范.md` 2 处冲突条文，并写明是旧版沿革）。两处**已被 md 取代、以 md 为准**的条文，留在这里作为命名规范沿革的证据：

1. `代码规范.docx` 说「文件名、函数名、变量名使用蛇形命名法」并举例 `student_age`；现行 `代码规范.md` §1.1 规定**文件名用 PascalCase**（与代码实测一致：`client/`、`practice/` 下 55 个 `.h/.cpp` 中 51 个是 PascalCase）。md 自己在 `11a18aa`（2026-06-06）之前也曾是 snake_case。
2. `代码规范.docx` 说「单元测试统一放在 `tests` 文件夹中」；现行规范与实际目录都是 **`test/`**（单数）。

## 真源在哪

**`docs/markdown/*.md` 是唯一真源。** 详见 `docs/adr/0003-文档真源.md`。

## 相关历史

`docs/docx/设计文档.docx` 曾与本目录两个文件放在一起。它是 **0 字节**，并且**从 2026-05-01 创建之日起就是空的**（`git log --all --follow --name-status` 只有 1 次 `A` 与 2 次 `R100`，没有任何 `M`；三处 blob 均为空 blob `e69de29bb2d1d6434b8b29ae775ad8c2e48c5391`），无内容可恢复，已删除。与它同名的那份 `docs/markdown/设计文档.md`（2026-06-10 被 `aa3e210` 删除）已从 git 恢复。
