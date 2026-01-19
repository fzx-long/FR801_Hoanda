#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
命令解析器
用于加载和管理命令数据库
"""

import json
import os

class CommandParser:
    def __init__(self):
        self.commands_data = None
        self.command_map = {}
    
    def load_commands(self, file_path):
        """
        加载命令数据库
        
        参数：
            file_path: 命令数据库文件路径
        
        返回：
            是否加载成功
        """
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                self.commands_data = json.load(f)
            
            # 构建命令映射，方便快速查找
            self._build_command_map()
            return True
        except Exception as e:
            print(f"加载命令数据库失败: {e}")
            return False
    
    def _build_command_map(self):
        """
        构建命令ID到命令信息的映射
        """
        if not self.commands_data:
            return
        
        self.command_map.clear()
        
        for category_name, category in self.commands_data.get('categories', {}).items():
            for cmd in category.get('commands', []):
                cmd_id = cmd.get('id')
                if cmd_id:
                    self.command_map[cmd_id] = {
                        'id': cmd_id,
                        'hex': cmd.get('hex'),
                        'name': cmd.get('name'),
                        'description': cmd.get('description'),
                        'category': category_name,
                        'category_name': category.get('name')
                    }
    
    def get_all_categories(self):
        """
        获取所有命令分类
        
        返回：
            分类列表，每个元素包含name和key
        """
        if not self.commands_data:
            return []
        
        categories = []
        for key, category in self.commands_data.get('categories', {}).items():
            categories.append({
                'key': key,
                'name': category.get('name')
            })
        return categories
    
    def get_commands_by_category(self, category_key):
        """
        获取指定分类的所有命令
        
        参数：
            category_key: 分类键
        
        返回：
            命令列表
        """
        if not self.commands_data:
            return []
        
        category = self.commands_data.get('categories', {}).get(category_key)
        if not category:
            return []
        
        return category.get('commands', [])
    
    def get_command_by_id(self, cmd_id):
        """
        根据命令ID获取命令信息
        
        参数：
            cmd_id: 命令ID
        
        返回：
            命令信息字典
        """
        return self.command_map.get(cmd_id)
    
    def search_commands(self, keyword):
        """
        搜索命令
        
        参数：
            keyword: 搜索关键词
        
        返回：
            匹配的命令列表
        """
        if not self.commands_data:
            return []
        
        matches = []
        keyword = keyword.lower()
        
        for category_name, category in self.commands_data.get('categories', {}).items():
            for cmd in category.get('commands', []):
                if (keyword in cmd.get('name', '').lower() or
                    keyword in cmd.get('description', '').lower() or
                    keyword in category.get('name', '').lower()):
                    matches.append({
                        'id': cmd.get('id'),
                        'hex': cmd.get('hex'),
                        'name': cmd.get('name'),
                        'description': cmd.get('description'),
                        'category': category_name,
                        'category_name': category.get('name')
                    })
        
        return matches
    
    def get_all_commands(self):
        """
        获取所有命令
        
        返回：
            命令列表
        """
        if not self.commands_data:
            return []
        
        all_commands = []
        for category in self.commands_data.get('categories', {}).values():
            all_commands.extend(category.get('commands', []))
        return all_commands

# 全局命令解析器实例
command_parser = CommandParser()

# 加载默认命令数据库
def load_default_commands():
    """
    加载默认命令数据库
    """
    # 查找默认命令数据库文件
    current_dir = os.path.dirname(os.path.dirname(__file__))
    commands_file = os.path.join(current_dir, 'data', 'commands.json')
    
    if os.path.exists(commands_file):
        return command_parser.load_commands(commands_file)
    else:
        print(f"默认命令数据库文件不存在: {commands_file}")
        return False

# 初始化时加载默认命令数据库
load_default_commands()
