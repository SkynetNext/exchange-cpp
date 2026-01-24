#!/usr/bin/env python3
"""
生成延迟分解对比图 - 瀑布图风格 (Waterfall Chart)
业界最佳实践：
- ColorBrewer 配色方案 (colorblind-safe)
- 清晰展示顺序执行的延迟分解
"""

import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np
import os

# 设置中文字体
plt.rcParams['font.sans-serif'] = ['SimHei', 'Microsoft YaHei', 'Arial Unicode MS', 'DejaVu Sans']
plt.rcParams['axes.unicode_minus'] = False

# 数据定义 (单位: 微秒) - 按执行顺序，包含全部阶段
stages = [
    'Token验证',
    '冻结检查', 
    '网络→ME',
    '撮合执行',
    '网络←ME',
    '清算处理',
    'Raft同步'
]

dpdk_values = [0.5, 0.5, 9.0, 1.0, 9.0, 0.5, 36.0]   # 总计 56.5μs
rdma_values = [0.5, 0.5, 1.5, 1.0, 1.5, 0.5, 3.0]    # 总计 8.5μs

# ============================================================
# ColorBrewer 配色方案 - Set2 (qualitative, colorblind-safe)
# https://colorbrewer2.org/#type=qualitative&scheme=Set2&n=4
# ============================================================
# 按功能类别分组:
#   - 本地处理: 浅色 (快速，非瓶颈)
#   - 网络传输: 中等饱和度 (关键路径)
#   - 撮合执行: 深色 (核心处理)
#   - Raft同步: 强调色 (一致性开销)

# ColorBrewer Set2 palette
CB_TEAL = '#66c2a5'      # 青绿 - 本地处理
CB_ORANGE = '#fc8d62'    # 橙色 - 网络传输 (瓶颈高亮)
CB_PURPLE = '#8da0cb'    # 紫蓝 - 撮合执行
CB_PINK = '#e78ac3'      # 粉色 - Raft同步 (一致性)

# 每个阶段的颜色映射
stage_colors = {
    'Token验证': CB_TEAL,
    '冻结检查': CB_TEAL,
    '网络→ME': CB_ORANGE,
    '撮合执行': CB_PURPLE,
    '网络←ME': CB_ORANGE,
    '清算处理': CB_TEAL,
    'Raft同步': CB_PINK
}

# 类别颜色 (用于图例)
category_colors = {
    '本地处理': CB_TEAL,
    '网络传输': CB_ORANGE,
    '撮合执行': CB_PURPLE,
    'Raft同步': CB_PINK
}

def create_waterfall_chart():
    """创建瀑布图风格的延迟分解对比 - 上下排列便于直接对比"""
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 8), sharex=True)
    
    bar_height = 0.6
    y_pos = np.arange(len(stages))
    
    dpdk_total = sum(dpdk_values)
    rdma_total = sum(rdma_values)
    max_x = dpdk_total * 1.08
    
    # === 上图: DPDK ===
    cumulative = 0
    for i, (stage, val) in enumerate(zip(stages, dpdk_values)):
        ax1.barh(i, val, left=cumulative, height=bar_height,
                color=stage_colors[stage], edgecolor='white', linewidth=0.5)
        
        # 数值标注
        if val >= 5:
            ax1.text(cumulative + val/2, i, f'{val:.1f}μs', 
                    ha='center', va='center', fontsize=10, fontweight='bold', color='white')
        elif val >= 1.0:
            ax1.text(cumulative + val/2, i, f'{val:.1f}', 
                    ha='center', va='center', fontsize=9, fontweight='bold', color='white')
        else:
            ax1.text(cumulative + val + 0.3, i, f'{val:.1f}', 
                    ha='left', va='center', fontsize=8, color='#666')
        cumulative += val
    
    ax1.set_xlim(0, max_x)
    ax1.set_yticks(y_pos)
    ax1.set_yticklabels(stages, fontsize=10)
    ax1.set_title(f'Aeron DPDK  —  总计: {dpdk_total:.1f}μs', fontsize=12, fontweight='bold', loc='left', pad=10)
    ax1.grid(axis='x', alpha=0.3, linestyle='--')
    ax1.axvline(x=dpdk_total, color='#d62728', linestyle='--', linewidth=2, alpha=0.7)
    ax1.text(dpdk_total + 0.5, 3, f'{dpdk_total:.1f}μs', color='#d62728', fontsize=10, fontweight='bold', va='center')
    ax1.invert_yaxis()
    
    # === 下图: RDMA ===
    cumulative = 0
    for i, (stage, val) in enumerate(zip(stages, rdma_values)):
        ax2.barh(i, val, left=cumulative, height=bar_height,
                color=stage_colors[stage], edgecolor='white', linewidth=0.5)
        
        if val >= 1.5:
            ax2.text(cumulative + val/2, i, f'{val:.1f}', 
                    ha='center', va='center', fontsize=9, fontweight='bold', color='white')
        elif val >= 1.0:
            ax2.text(cumulative + val/2, i, f'{val:.1f}', 
                    ha='center', va='center', fontsize=9, fontweight='bold', color='#333')
        else:
            ax2.text(cumulative + val + 0.2, i, f'{val:.1f}', 
                    ha='left', va='center', fontsize=8, color='#666')
        cumulative += val
    
    ax2.set_xlim(0, max_x)
    ax2.set_yticks(y_pos)
    ax2.set_yticklabels(stages, fontsize=10)
    ax2.set_xlabel('延迟 (μs)', fontsize=11)
    ax2.set_title(f'RDMA Two-sided  —  总计: {rdma_total:.1f}μs', fontsize=12, fontweight='bold', loc='left', pad=10)
    ax2.grid(axis='x', alpha=0.3, linestyle='--')
    ax2.axvline(x=rdma_total, color='#2ca02c', linestyle='--', linewidth=2, alpha=0.7)
    ax2.text(rdma_total + 0.5, 3, f'{rdma_total:.1f}μs', color='#2ca02c', fontsize=10, fontweight='bold', va='center')
    ax2.invert_yaxis()
    
    # === 图例 (右上角) ===
    legend_elements = [
        mpatches.Patch(facecolor=CB_TEAL, label='本地处理', edgecolor='white'),
        mpatches.Patch(facecolor=CB_ORANGE, label='网络传输', edgecolor='white'),
        mpatches.Patch(facecolor=CB_PURPLE, label='撮合执行', edgecolor='white'),
        mpatches.Patch(facecolor=CB_PINK, label='Raft同步', edgecolor='white'),
    ]
    ax1.legend(handles=legend_elements, loc='upper right', ncol=4, fontsize=9, framealpha=0.95)
    
    # === 性能提升标注 ===
    improvement = dpdk_total / rdma_total
    fig.suptitle(f'端到端延迟分解对比 (P99, 含 Raft)    性能提升: {improvement:.1f}x', 
                fontsize=14, fontweight='bold', y=0.98)
    
    plt.tight_layout(rect=[0, 0, 1, 0.95])
    
    # 保存
    script_dir = os.path.dirname(os.path.abspath(__file__))
    docs_dir = os.path.dirname(script_dir)
    images_dir = os.path.join(docs_dir, 'images')
    os.makedirs(images_dir, exist_ok=True)
    
    output_path = os.path.join(images_dir, 'latency_breakdown_comparison.png')
    plt.savefig(output_path, dpi=300, bbox_inches='tight', facecolor='white')
    print(f'图表已保存到: {output_path}')
    
    svg_path = os.path.join(images_dir, 'latency_breakdown_comparison.svg')
    plt.savefig(svg_path, format='svg', bbox_inches='tight', facecolor='white')
    print(f'SVG 图表已保存到: {svg_path}')
    
    plt.close()


if __name__ == '__main__':
    print('生成延迟分解瀑布图 (ColorBrewer 配色)...')
    create_waterfall_chart()
    print('\n完成！')
