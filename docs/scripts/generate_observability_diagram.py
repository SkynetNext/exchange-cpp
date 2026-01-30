#!/usr/bin/env python3
"""
生成低延迟可观测性架构图 (Tracing Layer + Collection Layer)。
输出到 docs/images/observability_architecture.png
架构：Gateway 进程 | Trading+Clearing 进程 (同机 Aeron IPC) | Matching 进程 (Aeron)
依赖: pip install matplotlib
"""

import os
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch
import matplotlib.patheffects as path_effects

# ============================================================================
# 风格常量 (AWS-style)
# ============================================================================
BG_COLOR = "#fafafa"
GROUP_BG_TRACING = "#e8f4fc"
GROUP_BG_COLLECTION = "#fef6e8"
GROUP_BORDER = "#c9dae8"
HOST_BG = "#f5faff"              # 同机分组背景
HOST_BORDER = "#b0c4de"
PROCESS_BG = "#ffffff"
PROCESS_BORDER = "#232f3e"
BOX_SHADOW = "#d0d0d0"
AERON_COLOR = "#0073bb"
AERON_IPC_COLOR = "#2e8b57"      # IPC 用绿色区分
SHARED_MEM_COLOR = "#ec7211"
REPORT_COLOR = "#545b64"
FONT_NAME = "sans-serif"
TITLE_SIZE = 16
LAYER_LABEL_SIZE = 12
LABEL_SIZE = 10
SMALL_LABEL = 9
DPI = 250
FIG_W, FIG_H = 15, 8.5
BOX_RADIUS = 0.12
SHADOW_OFFSET = 0.04


def draw_box(ax, x, y, w, h, label_lines, shadow=True):
    """绘制圆角矩形框。"""
    if shadow:
        s = FancyBboxPatch(
            (x + SHADOW_OFFSET, y - SHADOW_OFFSET), w, h,
            boxstyle=f"round,pad=0,rounding_size={BOX_RADIUS}",
            facecolor=BOX_SHADOW, edgecolor="none", zorder=1
        )
        ax.add_patch(s)
    box = FancyBboxPatch(
        (x, y), w, h,
        boxstyle=f"round,pad=0,rounding_size={BOX_RADIUS}",
        facecolor=PROCESS_BG, edgecolor=PROCESS_BORDER, linewidth=1.5, zorder=2
    )
    ax.add_patch(box)
    mid_y = y + h / 2
    if len(label_lines) == 1:
        ax.text(x + w / 2, mid_y, label_lines[0], fontsize=LABEL_SIZE,
                ha="center", va="center", fontfamily=FONT_NAME, zorder=3)
    else:
        gap = 0.18
        top = mid_y + gap * (len(label_lines) - 1) / 2
        for i, line in enumerate(label_lines):
            ax.text(x + w / 2, top - i * gap, line, fontsize=SMALL_LABEL,
                    ha="center", va="center", fontfamily=FONT_NAME, zorder=3)
    return x + w / 2  # 返回中心 x


def draw_group(ax, x, y, w, h, label, color, border, label_pos="top"):
    """绘制分组背景。"""
    bg = FancyBboxPatch(
        (x, y), w, h, boxstyle="round,pad=0,rounding_size=0.2",
        facecolor=color, edgecolor=border, linewidth=1.2, zorder=0
    )
    ax.add_patch(bg)
    if label_pos == "top":
        ax.text(x + w / 2, y + h - 0.25, label, fontsize=LAYER_LABEL_SIZE,
                ha="center", va="top", fontweight="bold", color="#232f3e", fontfamily=FONT_NAME, zorder=1)
    else:
        ax.text(x + w / 2, y + h - 0.12, label, fontsize=SMALL_LABEL - 1,
                ha="center", va="top", color="#555", fontfamily=FONT_NAME, fontstyle="italic", zorder=1)


def draw_arrow(ax, start, end, color, style="solid", label=None, offset=(0, 0.15)):
    """绘制箭头。"""
    arrow = FancyArrowPatch(
        start, end, arrowstyle="-|>,head_width=0.1,head_length=0.07",
        connectionstyle="arc3,rad=0", color=color,
        linewidth=2.0 if style == "solid" else 1.6,
        linestyle="-" if style == "solid" else (0, (5, 3)), zorder=4, mutation_scale=11
    )
    ax.add_patch(arrow)
    if label:
        mx, my = (start[0] + end[0]) / 2 + offset[0], (start[1] + end[1]) / 2 + offset[1]
        t = ax.text(mx, my, label, fontsize=SMALL_LABEL - 0.5, ha="center", va="center",
                    color=color, fontweight="bold", fontfamily=FONT_NAME, zorder=5)
        t.set_path_effects([path_effects.Stroke(linewidth=3, foreground="white"), path_effects.Normal()])


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    out_path = os.path.join(os.path.dirname(script_dir), "images", "observability_architecture")
    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    fig, ax = plt.subplots(figsize=(FIG_W, FIG_H), facecolor=BG_COLOR)
    ax.set_facecolor(BG_COLOR)
    ax.set_aspect("equal")
    ax.axis("off")
    ax.set_xlim(0, 15)
    ax.set_ylim(0, 8.5)

    # ========================================================================
    # 布局：3 进程，紧凑排列，留出右侧空间给 Aggregator
    # ========================================================================
    proc_w, proc_h, proc_gap = 2.8, 2.4, 0.6
    box_w, box_h = 2.2, 0.85
    start_x = 1.0  # 从左侧开始

    tracing_y, tracing_h = 4.3, 3.6
    coll_y, coll_h = 0.5, 3.0

    # ========================================================================
    # Tracing Layer
    # ========================================================================
    draw_group(ax, 0.4, tracing_y, 14.2, tracing_h, "Tracing Layer", GROUP_BG_TRACING, GROUP_BORDER)

    # --- 同机分组 (Gateway + Trading/Clearing) ---
    host_x = start_x - 0.2
    host_w = 2 * proc_w + proc_gap + 0.4
    host_y = tracing_y + 0.35
    host_h = proc_h + 0.5
    draw_group(ax, host_x, host_y, host_w, host_h, "Same Host (Aeron IPC)", HOST_BG, HOST_BORDER, label_pos="bottom")

    proc_y = host_y + 0.35
    box_y = proc_y + (proc_h - box_h) / 2
    mid_y = box_y + box_h / 2

    # Process 1: Gateway
    p1_x = start_x
    c1 = draw_box(ax, p1_x + (proc_w - box_w) / 2, box_y, box_w, box_h, ["Gateway", "(T1 in / T6 out)"])
    p1_right = p1_x + proc_w

    # Process 2: Trading + Clearing
    p2_x = p1_x + proc_w + proc_gap
    c2 = draw_box(ax, p2_x + (proc_w - box_w) / 2, box_y, box_w, box_h, ["Trading + Clearing", "(T2 out / T5 in)"])
    p2_left, p2_right = p2_x, p2_x + proc_w

    # Process 3: Matching Engine
    p3_x = p2_x + proc_w + proc_gap
    c3 = draw_box(ax, p3_x + (proc_w - box_w) / 2, box_y, box_w, box_h, ["Matching Engine", "(T3 in / T4 out)"])
    p3_left = p3_x

    # --- Aeron IPC (Gateway ↔ Trading+Clearing) ---
    draw_arrow(ax, (p1_right - 0.25, mid_y + 0.12), (p2_left + 0.25, mid_y + 0.12), AERON_IPC_COLOR, label="Aeron IPC", offset=(0, 0.22))
    draw_arrow(ax, (p2_left + 0.25, mid_y - 0.12), (p1_right - 0.25, mid_y - 0.12), AERON_IPC_COLOR, offset=(0, -0.22))

    # --- Aeron (Trading+Clearing ↔ Matching) ---
    draw_arrow(ax, (p2_right - 0.25, mid_y + 0.12), (p3_left + 0.25, mid_y + 0.12), AERON_COLOR, label="Aeron", offset=(0, 0.22))
    draw_arrow(ax, (p3_left + 0.25, mid_y - 0.12), (p2_right - 0.25, mid_y - 0.12), AERON_COLOR, offset=(0, -0.22))

    # ========================================================================
    # Collection Layer
    # ========================================================================
    draw_group(ax, 0.4, coll_y, 14.2, coll_h, "Collection Layer", GROUP_BG_COLLECTION, "#f5deb3")

    coll_box_w, coll_box_h = 2.0, 0.8
    coll_box_y = coll_y + 1.0
    mid_coll_y = coll_box_y + coll_box_h / 2

    # Agent G (对应同机 Gateway + Trading/Clearing) - 居中于 host 分组下方
    host_center = host_x + host_w / 2
    xg = host_center - coll_box_w / 2
    draw_box(ax, xg, coll_box_y, coll_box_w, coll_box_h, ["Agent G"])
    pg_right = xg + coll_box_w
    pg_top = coll_box_y + coll_box_h

    # Agent M (对应 Matching) - 居中于 Matching 进程下方
    me_center = p3_x + proc_w / 2
    xm = me_center - coll_box_w / 2
    draw_box(ax, xm, coll_box_y, coll_box_w, coll_box_h, ["Agent M"])
    pm_right = xm + coll_box_w
    pm_top = coll_box_y + coll_box_h

    # Aggregator - 放在最右侧，与 Agent M 保持足够间距
    agg_w = 2.0
    xa = pm_right + 1.5  # 动态计算位置，确保不重叠
    draw_box(ax, xa, coll_box_y, agg_w, coll_box_h, ["Aggregator"])
    pa_left = xa

    # --- Shared Memory ---
    draw_arrow(ax, (host_center, host_y), (host_center, pg_top), SHARED_MEM_COLOR, style="dashed", label="Shared Mem", offset=(-0.9, 0.05))
    draw_arrow(ax, (me_center, proc_y), (me_center, pm_top), SHARED_MEM_COLOR, style="dashed", label="Shared Mem", offset=(0.9, 0.05))

    # --- Reporting ---
    draw_arrow(ax, (pg_right + 0.05, mid_coll_y + 0.06), (pa_left - 0.05, mid_coll_y + 0.06), REPORT_COLOR)
    draw_arrow(ax, (pm_right + 0.05, mid_coll_y - 0.06), (pa_left - 0.05, mid_coll_y - 0.06), REPORT_COLOR)
    ax.text((pm_right + pa_left) / 2, mid_coll_y + 0.35, "Reporting", fontsize=SMALL_LABEL,
            ha="center", va="center", color=REPORT_COLOR, fontweight="bold", fontfamily=FONT_NAME)

    # ========================================================================
    # 标题
    # ========================================================================
    fig.suptitle("NanoTrace - Low Latency Observability Architecture", fontsize=TITLE_SIZE, fontweight="bold", y=0.97, color="#232f3e")

    plt.tight_layout(rect=[0, 0, 1, 0.95])
    plt.savefig(out_path + ".png", dpi=DPI, bbox_inches="tight", facecolor=BG_COLOR)
    plt.close()
    print(f"图表已保存到: {out_path}.png")


if __name__ == "__main__":
    main()
