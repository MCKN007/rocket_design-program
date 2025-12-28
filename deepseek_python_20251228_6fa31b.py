import subprocess
import os

# 首先确保cea目录存在
if not os.path.exists('cea'):
    os.makedirs('cea')
    print("创建了cea目录")

# 切换到CEA目录
original_dir = os.getcwd()
try:
    os.chdir('cea')
except FileNotFoundError:
    print("CEA目录不存在，已创建")
    os.makedirs('cea', exist_ok=True)
    os.chdir('cea')

# 创建输入文件
with open('thermo.inp', 'w') as f:
    f.write("""problem rocket
p,bar=10
o/f=2.5
supar=40
react
fuel=RP-1 wt=100 t,k=298.15
oxid=O2 wt=100 t,k=298.15
output short
end""")

print("输入文件已创建")

# 运行CEA
print("运行CEA...")
proc = subprocess.Popen(['FCEA2.exe'], 
                       stdin=subprocess.PIPE,
                       stdout=subprocess.PIPE, 
                       stderr=subprocess.PIPE,
                       text=True, 
                       encoding='utf-8',
                       bufsize=1,
                       universal_newlines=True)

# 发送文件名（不带扩展名）
stdout, stderr = proc.communicate(input='thermo\n', timeout=30)

print("退出代码:", proc.returncode)
print("标准输出前500字符:")
print(stdout[:500])
print("错误输出:")
print(stderr)

# 检查输出文件
if os.path.exists('thermo.out'):
    size = os.path.getsize('thermo.out')
    print(f"输出文件大小: {size} 字节")
    if size > 1000:
        with open('thermo.out', 'r') as f:
            content = f.read(1000)
            print("前1000字符:")
            print(content)
    else:
        print("输出文件太小，可能计算失败")
        with open('thermo.out', 'r') as f:
            print("完整内容:")
            print(f.read())
else:
    print("输出文件不存在")

# 返回原目录
os.chdir(original_dir)