# Guía de Implementación Paso a Paso - Sistema FIFO

## Tabla de Contenidos
1. [SQLite vs JSON: ¿Por qué usar base de datos?](#sqlite-vs-json)
2. [Requisitos del Sistema](#requisitos-del-sistema)
3. [Configuración del Entorno de Desarrollo](#configuración-del-entorno)
4. [Implementación Paso a Paso](#implementación-paso-a-paso)
5. [Prototipo Funcional](#prototipo-funcional)

---

## SQLite vs JSON: ¿Por qué usar base de datos?

### ¿Por qué NO usar JSON en almacenamiento local?

#### Limitaciones de JSON para este proyecto:

**1. Problemas de Concurrencia**
```
Escenario: El servicio está escribiendo datos mientras la GUI lee
JSON: ❌ Riesgo de corrupción de archivo
      ❌ Necesitas implementar locks manualmente
      ❌ Posible pérdida de datos
      
SQLite: ✅ Maneja concurrencia automáticamente
        ✅ Transacciones ACID garantizadas
        ✅ Múltiples lectores simultáneos
```

**2. Rendimiento con Datos Grandes**
```
Escenario: Tienes 10,000 registros de archivos
JSON: ❌ Debe cargar TODO el archivo en memoria
      ❌ Modificar UN registro = reescribir TODO
      ❌ Buscar dato viejo = recorrer todos los registros
      
SQLite: ✅ Carga solo lo que necesitas
        ✅ Actualiza solo el registro específico
        ✅ Índices para búsquedas rápidas (milisegundos)
```

**3. Consultas Complejas FIFO**
```python
# Con JSON - COMPLICADO Y LENTO
with open('data.json', 'r') as f:
    data = json.load(f)  # Carga todo en memoria

# Buscar archivos más viejos de un asset específico
oldest_files = [
    f for f in data['files'] 
    if f['asset_id'] == 'asset_001' 
    and f['status'] == 'ACTIVE'
]
oldest_files.sort(key=lambda x: x['timestamp'])  # Ordenar manualmente
files_to_delete = oldest_files[:10]

# Con SQLite - SIMPLE Y RÁPIDO
cursor.execute("""
    SELECT * FROM data_files 
    WHERE asset_id = ? AND status = 'ACTIVE'
    ORDER BY created_timestamp ASC 
    LIMIT 10
""", ('asset_001',))
files_to_delete = cursor.fetchall()
```

**4. Integridad de Datos**
```
JSON: 
❌ Sin validación de tipos
❌ Sin relaciones entre tablas
❌ Sin constraints (restricciones)
❌ Fácil corromper datos manualmente

SQLite:
✅ Tipos de datos validados
✅ Foreign keys (llaves foráneas)
✅ Constraints automáticos
✅ Rollback si algo falla
```

**5. Consultas Analíticas**
```sql
-- ¿Cuánto espacio ha liberado cada asset este mes?
-- Con SQLite: UNA CONSULTA
SELECT 
    asset_id, 
    SUM(space_freed_mb) as total_freed,
    COUNT(*) as deletion_count
FROM deletion_log
WHERE deletion_timestamp >= date('now', '-30 days')
GROUP BY asset_id;

-- Con JSON: Tienes que programar esto manualmente
-- Iterar todos los registros, filtrar fechas, agrupar, sumar...
-- 50+ líneas de código Python vs 1 consulta SQL
```

### Ventajas Específicas de SQLite para este Proyecto

| Característica | Importancia | Explicación |
|----------------|-------------|-------------|
| **Cero Configuración** | ⭐⭐⭐⭐⭐ | No necesita servidor, es un archivo .db |
| **Transacciones** | ⭐⭐⭐⭐⭐ | Si falla el borrado, se revierte todo |
| **Índices** | ⭐⭐⭐⭐⭐ | Búsqueda de archivos viejos en <1ms |
| **Joins** | ⭐⭐⭐⭐ | Relacionar assets con archivos fácilmente |
| **Backup Fácil** | ⭐⭐⭐⭐ | Copiar un archivo = backup completo |
| **Tamaño** | ⭐⭐⭐⭐⭐ | Librería incluida en Python (0 bytes extra) |

### Comparación Real: Borrado FIFO

**Escenario:** Borrar archivos viejos de asset_001 hasta liberar 500MB

#### Con JSON (Pseudo-código):
```python
import json
import os
import fcntl  # Para locks

# 1. Adquirir lock (evitar conflictos)
with open('data.json', 'r+') as f:
    fcntl.flock(f, fcntl.LOCK_EX)  # Lock exclusivo
    
    # 2. Leer TODO el archivo JSON
    data = json.load(f)
    
    # 3. Filtrar manualmente
    asset_files = [
        x for x in data['files'] 
        if x['asset_id'] == 'asset_001' and x['status'] == 'ACTIVE'
    ]
    
    # 4. Ordenar manualmente (por timestamp)
    asset_files.sort(key=lambda x: x['timestamp'])
    
    # 5. Iterar y marcar para borrado
    space_freed = 0
    for file in asset_files:
        if space_freed >= 500:
            break
        file['status'] = 'DELETED'
        file['deleted_at'] = datetime.now().isoformat()
        space_freed += file['size_mb']
    
    # 6. Reescribir TODO el archivo
    f.seek(0)
    f.truncate()
    json.dump(data, f, indent=2)
    
    fcntl.flock(f, fcntl.LOCK_UN)  # Liberar lock

# Problemas:
# - Si hay 50,000 registros, carga TODOS en memoria
# - Reescribe TODO el archivo (lento)
# - Lock bloquea TODA la aplicación
# - Si falla a mitad: datos corruptos
```

#### Con SQLite:
```python
import sqlite3

conn = sqlite3.connect('fifo.db')
cursor = conn.cursor()

try:
    # 1. Iniciar transacción (automático)
    cursor.execute("""
        WITH files_to_delete AS (
            SELECT file_id, file_size_mb
            FROM data_files
            WHERE asset_id = ? AND status = 'ACTIVE'
            ORDER BY created_timestamp ASC
        )
        UPDATE data_files
        SET status = 'DELETED', deleted_timestamp = datetime('now')
        WHERE file_id IN (
            SELECT file_id FROM files_to_delete
            WHERE (
                SELECT SUM(file_size_mb) 
                FROM files_to_delete f2 
                WHERE f2.file_id <= files_to_delete.file_id
            ) <= 500
        )
    """, ('asset_001',))
    
    conn.commit()  # Si todo OK, guardar
    
except Exception as e:
    conn.rollback()  # Si falla, revertir TODO
    print(f"Error: {e}")

# Ventajas:
# - Solo toca los registros necesarios
# - Otras consultas pueden leer simultáneamente
# - Si falla, rollback automático
# - Código más limpio y mantenible
```

### ¿Cuándo SÍ usar JSON?

JSON es perfecto para:
- ✅ **Configuración estática** (como assets.json, storage_paths.json)
- ✅ **Datos pequeños** (<1000 registros)
- ✅ **Un solo proceso** escribiendo
- ✅ **No necesitas consultas complejas**

### Conclusión: SQLite es la Elección Correcta

Para este proyecto específico:
1. ✅ Múltiples procesos (servicio + GUI) leyendo/escribiendo
2. ✅ Miles de registros de archivos
3. ✅ Necesitas consultas FIFO rápidas
4. ✅ Requieres historial de borrados
5. ✅ Integridad de datos crítica

**SQLite te ahorra cientos de líneas de código y previene errores.**

---

## Requisitos del Sistema

### Software Necesario

#### 1. Python 3.9 o superior
```bash
# Verificar si está instalado
python --version

# Si no está instalado, descargar de:
# https://www.python.org/downloads/
```

#### 2. Librerías Python (Incluidas en instalación estándar)
```
✅ sqlite3      - Incluida en Python (no requiere instalación)
✅ os           - Incluida en Python
✅ datetime     - Incluida en Python
✅ json         - Incluida en Python
```

#### 3. Librerías Adicionales (para GUI y Servicio)
```bash
# Para la GUI
pip install PyQt5

# Para Windows Service
pip install pywin32

# Para logging mejorado (opcional)
pip install colorlog
```

### Hardware Mínimo

- **CPU:** Procesador de 2 núcleos
- **RAM:** 4 GB (el prototipo usa <100 MB)
- **Disco:** 10 GB libres para pruebas
- **OS:** Windows 10/11 o Windows Server 2016+

---

## Configuración del Entorno de Desarrollo

### Paso 1: Crear Estructura de Directorios

Abre PowerShell como Administrador y ejecuta:

```powershell
# Navegar a tu carpeta de trabajo
cd C:\Users\IDC INGENIERIA\fifo\FIFOImplementation

# Crear estructura de directorios
New-Item -ItemType Directory -Path ".\prototipo" -Force
New-Item -ItemType Directory -Path ".\prototipo\config" -Force
New-Item -ItemType Directory -Path ".\prototipo\database" -Force
New-Item -ItemType Directory -Path ".\prototipo\logs" -Force
New-Item -ItemType Directory -Path ".\prototipo\monitored_storage" -Force
New-Item -ItemType Directory -Path ".\prototipo\monitored_storage\asset_001" -Force
New-Item -ItemType Directory -Path ".\prototipo\monitored_storage\asset_002" -Force
New-Item -ItemType Directory -Path ".\prototipo\monitored_storage\asset_003" -Force

# Verificar estructura
tree .\prototipo /F
```

Deberías ver:
```
prototipo\
├── config\
├── database\
├── logs\
└── monitored_storage\
    ├── asset_001\
    ├── asset_002\
    └── asset_003\
```

### Paso 2: Verificar Python y SQLite

```powershell
# Verificar Python
python --version
# Salida esperada: Python 3.9.x o superior

# Verificar SQLite (incluido en Python)
python -c "import sqlite3; print('SQLite Version:', sqlite3.sqlite_version)"
# Salida esperada: SQLite Version: 3.x.x
```

### Paso 3: Instalar Dependencias

```powershell
# Crear entorno virtual (recomendado)
python -m venv venv

# Activar entorno virtual
.\venv\Scripts\Activate.ps1

# Si hay error de permisos, ejecutar primero:
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser

# Instalar dependencias
pip install PyQt5 pywin32 colorlog

# Verificar instalación
pip list
```

---

## Implementación Paso a Paso

### PASO 1: Crear la Base de Datos SQLite

#### 1.1 Entender el Esquema

```
┌─────────────────┐
│     ASSETS      │  ← Configuración de cada activo industrial
├─────────────────┤
│ asset_id (PK)   │
│ asset_name      │
│ average_weight  │
│ min_retention   │
│ max_retention   │
└─────────────────┘
         │
         │ 1:N (Un asset tiene muchos archivos)
         ▼
┌─────────────────┐
│   DATA_FILES    │  ← Registro de cada archivo (FIFO queue)
├─────────────────┤
│ file_id (PK)    │
│ asset_id (FK)   │
│ file_path       │
│ file_size_mb    │
│ created_ts      │  ← Usado para FIFO (ordenar por antigüedad)
│ status          │  ← 'ACTIVE' o 'DELETED'
└─────────────────┘
         │
         │ 1:1 (Cada archivo tiene un log de borrado)
         ▼
┌─────────────────┐
│  DELETION_LOG   │  ← Historial de borrados (auditoría)
├─────────────────┤
│ log_id (PK)     │
│ file_id (FK)    │
│ deletion_reason │
│ space_freed_mb  │
│ deletion_ts     │
└─────────────────┘
```

#### 1.2 Crear el Script de Inicialización

**Archivo: `prototipo\db_init.py`**

```python
import sqlite3
import os

def initialize_database(db_path):
    """
    Inicializa la base de datos SQLite con el esquema completo.
    
    Args:
        db_path: Ruta al archivo de base de datos
    """
    
    # Crear directorio si no existe
    os.makedirs(os.path.dirname(db_path), exist_ok=True)
    
    # Conectar (crea el archivo si no existe)
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    
    print("📦 Creando tablas...")
    
    # TABLA 1: Configuración de Assets
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS assets (
            asset_id TEXT PRIMARY KEY,
            asset_name TEXT NOT NULL,
            average_weight_mb REAL NOT NULL DEFAULT 50.0,
            minimum_retention_hours INTEGER NOT NULL DEFAULT 24,
            maximum_retention_days INTEGER NOT NULL DEFAULT 30,
            priority TEXT NOT NULL DEFAULT 'NORMAL',
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    """)
    print("  ✅ Tabla 'assets' creada")
    
    # TABLA 2: Rutas de almacenamiento monitoreadas
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS storage_paths (
            path_id INTEGER PRIMARY KEY AUTOINCREMENT,
            storage_path TEXT NOT NULL UNIQUE,
            total_capacity_gb REAL NOT NULL,
            warning_threshold REAL NOT NULL DEFAULT 0.75,
            critical_threshold REAL NOT NULL DEFAULT 0.90,
            is_monitored BOOLEAN NOT NULL DEFAULT 1,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    """)
    print("  ✅ Tabla 'storage_paths' creada")
    
    # TABLA 3: Registro de archivos (Cola FIFO)
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS data_files (
            file_id INTEGER PRIMARY KEY AUTOINCREMENT,
            asset_id TEXT NOT NULL,
            storage_path_id INTEGER NOT NULL,
            file_path TEXT NOT NULL UNIQUE,
            file_size_mb REAL NOT NULL,
            created_timestamp TIMESTAMP NOT NULL,
            ingested_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            status TEXT NOT NULL DEFAULT 'ACTIVE',
            deleted_timestamp TIMESTAMP,
            FOREIGN KEY (asset_id) REFERENCES assets(asset_id),
            FOREIGN KEY (storage_path_id) REFERENCES storage_paths(path_id)
        )
    """)
    print("  ✅ Tabla 'data_files' creada")
    
    # TABLA 4: Log de borrados (Auditoría)
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS deletion_log (
            log_id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_id INTEGER NOT NULL,
            asset_id TEXT NOT NULL,
            file_path TEXT NOT NULL,
            deletion_reason TEXT NOT NULL,
            space_freed_mb REAL NOT NULL,
            deletion_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            triggered_by TEXT NOT NULL,
            FOREIGN KEY (file_id) REFERENCES data_files(file_id),
            FOREIGN KEY (asset_id) REFERENCES assets(asset_id)
        )
    """)
    print("  ✅ Tabla 'deletion_log' creada")
    
    # ÍNDICES para consultas rápidas FIFO
    print("\n🔍 Creando índices para optimización...")
    
    # Índice para búsqueda FIFO (por asset y timestamp)
    cursor.execute("""
        CREATE INDEX IF NOT EXISTS idx_fifo_queue 
        ON data_files(asset_id, created_timestamp, status)
    """)
    print("  ✅ Índice FIFO creado")
    
    # Índice para búsqueda por ruta de storage
    cursor.execute("""
        CREATE INDEX IF NOT EXISTS idx_storage_path 
        ON data_files(storage_path_id, status)
    """)
    print("  ✅ Índice de storage creado")
    
    # Índice para log de borrados (por fecha)
    cursor.execute("""
        CREATE INDEX IF NOT EXISTS idx_deletion_timestamp 
        ON deletion_log(deletion_timestamp)
    """)
    print("  ✅ Índice de borrados creado")
    
    conn.commit()
    print("\n✅ Base de datos inicializada correctamente")
    
    # Mostrar información
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table'")
    tables = cursor.fetchall()
    print(f"\n📊 Tablas creadas: {len(tables)}")
    for table in tables:
        print(f"   - {table[0]}")
    
    conn.close()
    return True


def insert_sample_data(db_path):
    """
    Inserta datos de ejemplo para pruebas.
    """
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    
    print("\n📝 Insertando datos de ejemplo...")
    
    # Assets de ejemplo
    sample_assets = [
        ('ASSET_001', 'Sensor de Temperatura - Línea 1', 50.0, 2, 7, 'HIGH'),
        ('ASSET_002', 'Monitor de Presión - Línea 2', 75.0, 4, 14, 'NORMAL'),
        ('ASSET_003', 'Controlador PLC - Línea 3', 30.0, 1, 7, 'HIGH'),
    ]
    
    cursor.executemany("""
        INSERT OR IGNORE INTO assets 
        (asset_id, asset_name, average_weight_mb, minimum_retention_hours, 
         maximum_retention_days, priority)
        VALUES (?, ?, ?, ?, ?, ?)
    """, sample_assets)
    
    print(f"  ✅ {cursor.rowcount} assets insertados")
    
    # Storage path de ejemplo
    import os
    test_path = os.path.abspath("prototipo\\monitored_storage")
    
    cursor.execute("""
        INSERT OR IGNORE INTO storage_paths 
        (storage_path, total_capacity_gb, warning_threshold, critical_threshold)
        VALUES (?, ?, ?, ?)
    """, (test_path, 1.0, 0.75, 0.90))
    
    print(f"  ✅ Storage path configurado: {test_path}")
    
    conn.commit()
    conn.close()
    
    print("✅ Datos de ejemplo insertados")


if __name__ == "__main__":
    import os
    
    # Ruta de la base de datos
    db_path = os.path.join("prototipo", "database", "fifo_prototype.db")
    
    print("=" * 60)
    print("🚀 INICIALIZADOR DE BASE DE DATOS FIFO")
    print("=" * 60)
    
    # Verificar si ya existe
    if os.path.exists(db_path):
        response = input(f"\n⚠️  La base de datos ya existe en:\n{db_path}\n¿Deseas recrearla? (s/n): ")
        if response.lower() != 's':
            print("❌ Operación cancelada")
            exit(0)
        os.remove(db_path)
        print("🗑️  Base de datos anterior eliminada")
    
    # Crear base de datos
    initialize_database(db_path)
    
    # Insertar datos de ejemplo
    response = input("\n¿Deseas insertar datos de ejemplo? (s/n): ")
    if response.lower() == 's':
        insert_sample_data(db_path)
    
    print("\n" + "=" * 60)
    print("✅ PROCESO COMPLETADO")
    print("=" * 60)
    print(f"\n📍 Base de datos ubicada en:\n{os.path.abspath(db_path)}")
    print("\n🔧 Puedes explorar la base de datos con:")
    print("   - DB Browser for SQLite (https://sqlitebrowser.org/)")
    print("   - python -c \"import sqlite3; ...\"")
```

#### 1.3 Ejecutar la Inicialización

```powershell
# Navegar al directorio del prototipo
cd C:\Users\IDC INGENIERIA\fifo\FIFOImplementation

# Ejecutar el inicializador
python prototipo\db_init.py
```

**Salida esperada:**
```
============================================================
🚀 INICIALIZADOR DE BASE DE DATOS FIFO
============================================================

📦 Creando tablas...
  ✅ Tabla 'assets' creada
  ✅ Tabla 'storage_paths' creada
  ✅ Tabla 'data_files' creada
  ✅ Tabla 'deletion_log' creada

🔍 Creando índices para optimización...
  ✅ Índice FIFO creado
  ✅ Índice de storage creado
  ✅ Índice de borrados creado

✅ Base de datos inicializada correctamente

📊 Tablas creadas: 4
   - assets
   - storage_paths
   - data_files
   - deletion_log

¿Deseas insertar datos de ejemplo? (s/n): s

📝 Insertando datos de ejemplo...
  ✅ 3 assets insertados
  ✅ Storage path configurado
✅ Datos de ejemplo insertados
```

### PASO 2: Crear el Motor FIFO

#### 2.1 Entender la Lógica del Motor

```
┌─────────────────────────────────────────────────────────┐
│                    MOTOR FIFO                           │
│                                                         │
│  1. ANALIZAR ──> 2. EVALUAR ──> 3. DECIDIR ──> 4. BORRAR│
│                                                         │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐  ┌───────┐│
│  │ Nuevo    │   │¿Excede   │   │Calcular  │  │Borrar ││
│  │ archivo  │──>│ umbral?  │──>│espacio   │─>│FIFO   ││
│  │ detectado│   │¿Anormal? │   │necesario │  │viejos ││
│  └──────────┘   └──────────┘   └──────────┘  └───────┘│
└─────────────────────────────────────────────────────────┘
```

#### 2.2 Implementar el Motor

**Archivo: `prototipo\fifo_engine.py`**

```python
import sqlite3
import os
import shutil
from datetime import datetime, timedelta
from pathlib import Path

class FIFOEngine:
    """
    Motor principal del sistema FIFO para gestión de datos industriales.
    """
    
    def __init__(self, db_path):
        """
        Inicializa el motor FIFO.
        
        Args:
            db_path: Ruta a la base de datos SQLite
        """
        self.db_path = db_path
        self.conn = None
        self._connect()
    
    def _connect(self):
        """Establece conexión con la base de datos."""
        self.conn = sqlite3.connect(self.db_path)
        self.conn.row_factory = sqlite3.Row  # Permite acceder por nombre de columna
        print(f"✅ Conectado a base de datos: {self.db_path}")
    
    def close(self):
        """Cierra la conexión a la base de datos."""
        if self.conn:
            self.conn.close()
            print("🔌 Conexión cerrada")
    
    # ============================================================
    # PASO 1: ANALIZAR DATOS ENTRANTES
    # ============================================================
    
    def analyze_incoming_data(self, file_path, asset_id):
        """
        Analiza un archivo nuevo y extrae sus metadatos.
        
        Args:
            file_path: Ruta completa al archivo
            asset_id: Identificador del asset origen
            
        Returns:
            dict: Metadatos del archivo
        """
        print(f"\n📊 Analizando archivo entrante...")
        print(f"   Asset: {asset_id}")
        print(f"   Ruta: {file_path}")
        
        # Obtener tamaño del archivo
        file_size_bytes = os.path.getsize(file_path)
        file_size_mb = file_size_bytes / (1024 * 1024)
        
        # Obtener timestamp de creación
        created_timestamp = datetime.fromtimestamp(os.path.getctime(file_path))
        
        # Determinar storage path
        storage_path = self._get_storage_root(file_path)
        
        metadata = {
            'asset_id': asset_id,
            'file_path': file_path,
            'file_size_mb': file_size_mb,
            'created_timestamp': created_timestamp,
            'storage_path': storage_path
        }
        
        print(f"   Tamaño: {file_size_mb:.2f} MB")
        print(f"   Creado: {created_timestamp}")
        
        return metadata
    
    def _get_storage_root(self, file_path):
        """Obtiene la ruta raíz del storage monitoreado."""
        cursor = self.conn.cursor()
        cursor.execute("SELECT storage_path FROM storage_paths WHERE is_monitored = 1")
        
        monitored_paths = [row[0] for row in cursor.fetchall()]
        
        for monitored_path in monitored_paths:
            if file_path.startswith(monitored_path):
                return monitored_path
        
        # Si no está en paths monitoreados, usar el directorio padre
        return str(Path(file_path).parent.parent)
    
    # ============================================================
    # PASO 2: EVALUAR CRITERIOS DE BORRADO
    # ============================================================
    
    def evaluate_deletion_criteria(self, metadata):
        """
        Evalúa si se debe activar el borrado FIFO basándose en criterios.
        
        Args:
            metadata: Metadatos del archivo entrante
            
        Returns:
            dict: Decisión con prioridad y razones
        """
        print(f"\n🔍 Evaluando criterios de borrado...")
        
        asset_id = metadata['asset_id']
        incoming_size_mb = metadata['file_size_mb']
        storage_path = metadata['storage_path']
        
        # Obtener configuración del asset
        asset_config = self._get_asset_config(asset_id)
        if not asset_config:
            print(f"   ⚠️  Asset {asset_id} no configurado")
            return {'cleanup_required': False, 'priority': 'NONE', 'reasons': []}
        
        # Obtener configuración del storage
        storage_config = self._get_storage_config(storage_path)
        
        # Calcular uso actual del storage
        usage = shutil.disk_usage(storage_path)
        total_gb = usage.total / (1024**3)
        used_gb = usage.used / (1024**3)
        free_gb = usage.free / (1024**3)
        current_usage_pct = used_gb / total_gb
        
        # Calcular uso proyectado
        incoming_size_gb = incoming_size_mb / 1024
        projected_used_gb = used_gb + incoming_size_gb
        projected_usage_pct = projected_used_gb / total_gb
        
        print(f"   💾 Storage actual: {used_gb:.2f} GB / {total_gb:.2f} GB ({current_usage_pct*100:.1f}%)")
        print(f"   📈 Proyección: {projected_used_gb:.2f} GB ({projected_usage_pct*100:.1f}%)")
        
        decision = {
            'cleanup_required': False,
            'priority': 'LOW',
            'reasons': [],
            'space_needed_mb': 0
        }
        
        # CRITERIO 1: ¿Storage excederá umbral crítico?
        if projected_usage_pct >= storage_config['critical_threshold']:
            decision['cleanup_required'] = True
            decision['priority'] = 'CRITICAL'
            decision['reasons'].append('STORAGE_CRITICAL')
            decision['space_needed_mb'] = incoming_size_mb
            print(f"   🚨 CRÍTICO: Excede umbral {storage_config['critical_threshold']*100}%")
        
        # CRITERIO 2: ¿Archivo excede peso promedio?
        avg_weight = asset_config['average_weight_mb']
        weight_threshold = avg_weight * 1.2  # 120% del promedio
        
        if incoming_size_mb > weight_threshold:
            decision['cleanup_required'] = True
            if decision['priority'] != 'CRITICAL':
                decision['priority'] = 'HIGH'
            decision['reasons'].append('ABNORMAL_SIZE')
            decision['space_needed_mb'] = max(decision['space_needed_mb'], incoming_size_mb)
            print(f"   ⚠️  ANORMAL: {incoming_size_mb:.2f} MB > {weight_threshold:.2f} MB (promedio)")
        
        # CRITERIO 3: ¿Storage en zona de advertencia?
        if current_usage_pct >= storage_config['warning_threshold']:
            decision['cleanup_required'] = True
            if decision['priority'] == 'LOW':
                decision['priority'] = 'MEDIUM'
            decision['reasons'].append('STORAGE_WARNING')
            print(f"   ⚠️  ADVERTENCIA: Storage en {current_usage_pct*100:.1f}%")
        
        # CRITERIO 4 (Adicional): ¿Datos muy viejos?
        oldest_file_age = self._get_oldest_file_age(asset_id)
        if oldest_file_age and oldest_file_age > asset_config['maximum_retention_days']:
            decision['cleanup_required'] = True
            decision['reasons'].append('MAX_RETENTION_EXCEEDED')
            print(f"   ⏰ Hay datos con {oldest_file_age} días (max: {asset_config['maximum_retention_days']})")
        
        # Resultado
        if decision['cleanup_required']:
            print(f"   ✅ DECISIÓN: Borrado requerido (Prioridad: {decision['priority']})")
            print(f"   📋 Razones: {', '.join(decision['reasons'])}")
        else:
            print(f"   ✅ DECISIÓN: No se requiere borrado")
        
        return decision
    
    def _get_asset_config(self, asset_id):
        """Obtiene la configuración de un asset."""
        cursor = self.conn.cursor()
        cursor.execute("SELECT * FROM assets WHERE asset_id = ?", (asset_id,))
        row = cursor.fetchone()
        
        if row:
            return {
                'asset_id': row['asset_id'],
                'asset_name': row['asset_name'],
                'average_weight_mb': row['average_weight_mb'],
                'minimum_retention_hours': row['minimum_retention_hours'],
                'maximum_retention_days': row['maximum_retention_days'],
                'priority': row['priority']
            }
        return None
    
    def _get_storage_config(self, storage_path):
        """Obtiene la configuración de un storage path."""
        cursor = self.conn.cursor()
        cursor.execute("""
            SELECT * FROM storage_paths 
            WHERE storage_path = ? AND is_monitored = 1
        """, (storage_path,))
        
        row = cursor.fetchone()
        
        if row:
            return {
                'path_id': row['path_id'],
                'storage_path': row['storage_path'],
                'total_capacity_gb': row['total_capacity_gb'],
                'warning_threshold': row['warning_threshold'],
                'critical_threshold': row['critical_threshold']
            }
        
        # Si no existe configuración, crear una por defecto
        cursor.execute("""
            INSERT INTO storage_paths (storage_path, total_capacity_gb, warning_threshold, critical_threshold)
            VALUES (?, ?, ?, ?)
        """, (storage_path, 10.0, 0.75, 0.90))
        self.conn.commit()
        
        return self._get_storage_config(storage_path)
    
    def _get_oldest_file_age(self, asset_id):
        """Obtiene la edad del archivo más viejo de un asset (en días)."""
        cursor = self.conn.cursor()
        cursor.execute("""
            SELECT MIN(julianday('now') - julianday(created_timestamp)) as age_days
            FROM data_files
            WHERE asset_id = ? AND status = 'ACTIVE'
        """, (asset_id,))
        
        row = cursor.fetchone()
        return row[0] if row[0] else None
    
    # ============================================================
    # PASO 3: EJECUTAR BORRADO FIFO
    # ============================================================
    
    def execute_fifo_cleanup(self, asset_id, space_needed_mb, reason='MANUAL'):
        """
        Ejecuta el borrado FIFO: elimina archivos más viejos primero.
        
        Args:
            asset_id: Asset del cual borrar datos
            space_needed_mb: Espacio mínimo a liberar (MB)
            reason: Razón del borrado
            
        Returns:
            dict: Estadísticas del borrado
        """
        print(f"\n🗑️  Iniciando borrado FIFO...")
        print(f"   Asset: {asset_id}")
        print(f"   Espacio requerido: {space_needed_mb:.2f} MB")
        
        cursor = self.conn.cursor()
        
        # Obtener configuración del asset
        asset_config = self._get_asset_config(asset_id)
        min_retention_hours = asset_config['minimum_retention_hours']
        
        # Calcular timestamp mínimo de retención
        min_retention_timestamp = datetime.now() - timedelta(hours=min_retention_hours)
        
        try:
            # Obtener archivos ordenados por antigüedad (FIFO)
            cursor.execute("""
                SELECT file_id, file_path, file_size_mb, created_timestamp
                FROM data_files
                WHERE asset_id = ? 
                  AND status = 'ACTIVE'
                  AND created_timestamp < ?
                ORDER BY created_timestamp ASC
            """, (asset_id, min_retention_timestamp))
            
            files = cursor.fetchall()
            
            if not files:
                print(f"   ℹ️  No hay archivos elegibles para borrado (retención mínima: {min_retention_hours}h)")
                return {'success': False, 'files_deleted': 0, 'space_freed_mb': 0}
            
            print(f"   📁 Archivos candidatos: {len(files)}")
            
            space_freed_mb = 0.0
            files_deleted = 0
            deletion_list = []
            
            # Agregar buffer del 5% para seguridad
            space_target_mb = space_needed_mb * 1.05
            
            # Iterar archivos (del más viejo al más nuevo)
            for file_row in files:
                if space_freed_mb >= space_target_mb:
                    break
                
                file_id = file_row['file_id']
                file_path = file_row['file_path']
                file_size_mb = file_row['file_size_mb']
                created_ts = file_row['created_timestamp']
                
                deletion_list.append({
                    'file_id': file_id,
                    'file_path': file_path,
                    'file_size_mb': file_size_mb
                })
                
                space_freed_mb += file_size_mb
            
            print(f"   🎯 Archivos a borrar: {len(deletion_list)}")
            print(f"   💾 Espacio a liberar: {space_freed_mb:.2f} MB")
            
            # Confirmar antes de borrar
            if len(deletion_list) > 0:
                print(f"\n   ⚠️  Se borrarán {len(deletion_list)} archivo(s)")
                # En producción, aquí confirmarías o usarías modo automático
                
                # Ejecutar borrados
                for item in deletion_list:
                    file_id = item['file_id']
                    file_path = item['file_path']
                    file_size_mb = item['file_size_mb']
                    
                    # Borrar archivo físico
                    try:
                        if os.path.exists(file_path):
                            os.remove(file_path)
                            print(f"      ✅ Borrado: {os.path.basename(file_path)} ({file_size_mb:.2f} MB)")
                        else:
                            print(f"      ⚠️  No existe: {file_path}")
                    except Exception as e:
                        print(f"      ❌ Error borrando {file_path}: {e}")
                        continue
                    
                    # Actualizar estado en base de datos
                    cursor.execute("""
                        UPDATE data_files
                        SET status = 'DELETED', deleted_timestamp = ?
                        WHERE file_id = ?
                    """, (datetime.now(), file_id))
                    
                    # Registrar en log de borrados
                    cursor.execute("""
                        INSERT INTO deletion_log 
                        (file_id, asset_id, file_path, deletion_reason, space_freed_mb, triggered_by)
                        VALUES (?, ?, ?, ?, ?, ?)
                    """, (file_id, asset_id, file_path, reason, file_size_mb, 'FIFO_ENGINE'))
                    
                    files_deleted += 1
                
                # Commit de la transacción
                self.conn.commit()
                
                print(f"\n   ✅ Borrado completado:")
                print(f"      Archivos eliminados: {files_deleted}")
                print(f"      Espacio liberado: {space_freed_mb:.2f} MB")
                
                return {
                    'success': True,
                    'files_deleted': files_deleted,
                    'space_freed_mb': space_freed_mb
                }
            
        except Exception as e:
            print(f"   ❌ Error durante borrado: {e}")
            self.conn.rollback()
            return {'success': False, 'error': str(e)}
    
    # ============================================================
    # PASO 4: REGISTRAR ARCHIVO NUEVO
    # ============================================================
    
    def register_file(self, metadata):
        """
        Registra un archivo nuevo en la base de datos.
        
        Args:
            metadata: Metadatos del archivo
            
        Returns:
            int: ID del archivo registrado
        """
        print(f"\n📝 Registrando archivo en base de datos...")
        
        cursor = self.conn.cursor()
        
        # Obtener storage_path_id
        cursor.execute("""
            SELECT path_id FROM storage_paths WHERE storage_path = ?
        """, (metadata['storage_path'],))
        
        row = cursor.fetchone()
        storage_path_id = row[0] if row else 1
        
        # Insertar registro
        cursor.execute("""
            INSERT INTO data_files 
            (asset_id, storage_path_id, file_path, file_size_mb, created_timestamp)
            VALUES (?, ?, ?, ?, ?)
        """, (
            metadata['asset_id'],
            storage_path_id,
            metadata['file_path'],
            metadata['file_size_mb'],
            metadata['created_timestamp']
        ))
        
        self.conn.commit()
        file_id = cursor.lastrowid
        
        print(f"   ✅ Archivo registrado (ID: {file_id})")
        
        return file_id
    
    # ============================================================
    # UTILIDADES
    # ============================================================
    
    def get_active_files_count(self, asset_id=None):
        """Obtiene el conteo de archivos activos."""
        cursor = self.conn.cursor()
        
        if asset_id:
            cursor.execute("""
                SELECT COUNT(*) FROM data_files 
                WHERE asset_id = ? AND status = 'ACTIVE'
            """, (asset_id,))
        else:
            cursor.execute("SELECT COUNT(*) FROM data_files WHERE status = 'ACTIVE'")
        
        return cursor.fetchone()[0]
    
    def get_total_size(self, asset_id=None):
        """Obtiene el tamaño total de archivos activos."""
        cursor = self.conn.cursor()
        
        if asset_id:
            cursor.execute("""
                SELECT COALESCE(SUM(file_size_mb), 0) FROM data_files 
                WHERE asset_id = ? AND status = 'ACTIVE'
            """, (asset_id,))
        else:
            cursor.execute("""
                SELECT COALESCE(SUM(file_size_mb), 0) FROM data_files 
                WHERE status = 'ACTIVE'
            """)
        
        return cursor.fetchone()[0]
    
    def get_deletion_stats(self, days=7):
        """Obtiene estadísticas de borrados recientes."""
        cursor = self.conn.cursor()
        
        cursor.execute("""
            SELECT 
                asset_id,
                COUNT(*) as deletions,
                SUM(space_freed_mb) as total_freed_mb
            FROM deletion_log
            WHERE deletion_timestamp >= datetime('now', '-' || ? || ' days')
            GROUP BY asset_id
        """, (days,))
        
        return cursor.fetchall()


if __name__ == "__main__":
    # Prueba rápida
    db_path = os.path.join("prototipo", "database", "fifo_prototype.db")
    
    engine = FIFOEngine(db_path)
    
    print("\n" + "="*60)
    print("📊 ESTADÍSTICAS ACTUALES")
    print("="*60)
    print(f"Archivos activos totales: {engine.get_active_files_count()}")
    print(f"Tamaño total: {engine.get_total_size():.2f} MB")
    
    engine.close()
```

---

### PASO 3: Crear Generador de Datos Sintéticos

**Archivo: `prototipo\synthetic_data_generator.py`**

```python
import os
import random
from datetime import datetime, timedelta

def generate_synthetic_file(asset_id, size_mb, base_path, timestamp=None):
    """
    Genera un archivo sintético con datos aleatorios.
    
    Args:
        asset_id: ID del asset origen
        size_mb: Tamaño del archivo en MB
        base_path: Directorio base donde guardar
        timestamp: Timestamp personalizado (opcional)
        
    Returns:
        str: Ruta del archivo generado
    """
    
    # Crear directorio del asset si no existe
    asset_dir = os.path.join(base_path, asset_id)
    os.makedirs(asset_dir, exist_ok=True)
    
    # Generar nombre de archivo con timestamp
    if timestamp is None:
        timestamp = datetime.now()
    
    filename = f"data_{timestamp.strftime('%Y%m%d_%H%M%S')}_{random.randint(1000,9999)}.bin"
    file_path = os.path.join(asset_dir, filename)
    
    # Generar archivo con datos aleatorios
    print(f"🔨 Generando: {filename} ({size_mb} MB)")
    
    with open(file_path, 'wb') as f:
        # Escribir en bloques de 1 MB
        bytes_per_mb = 1024 * 1024
        for _ in range(int(size_mb)):
            f.write(os.urandom(bytes_per_mb))
    
    # Ajustar timestamp de creación (Windows)
    # Nota: En Windows es más complejo, se usa el timestamp actual
    
    print(f"   ✅ Generado: {file_path}")
    
    return file_path


def generate_batch(asset_id, num_files, size_range_mb, base_path, time_spread_hours=24):
    """
    Genera un lote de archivos con timestamps distribuidos.
    
    Args:
        asset_id: ID del asset
        num_files: Cantidad de archivos a generar
        size_range_mb: Tupla (min, max) de tamaños en MB
        base_path: Directorio base
        time_spread_hours: Dispersión temporal en horas
        
    Returns:
        list: Rutas de archivos generados
    """
    print(f"\n📦 Generando lote para {asset_id}:")
    print(f"   Archivos: {num_files}")
    print(f"   Rango de tamaño: {size_range_mb[0]}-{size_range_mb[1]} MB")
    print(f"   Dispersión temporal: {time_spread_hours} horas")
    
    files = []
    
    for i in range(num_files):
        # Tamaño aleatorio dentro del rango
        size_mb = random.uniform(size_range_mb[0], size_range_mb[1])
        
        # Timestamp aleatorio dentro de la dispersión
        hours_ago = random.uniform(0, time_spread_hours)
        timestamp = datetime.now() - timedelta(hours=hours_ago)
        
        # Generar archivo
        file_path = generate_synthetic_file(asset_id, size_mb, base_path, timestamp)
        files.append(file_path)
    
    print(f"\n✅ Lote completado: {len(files)} archivos generados")
    
    return files


if __name__ == "__main__":
    # Ejemplo de uso
    base_path = "prototipo\\monitored_storage"
    
    print("="*60)
    print("🧪 GENERADOR DE DATOS SINTÉTICOS")
    print("="*60)
    
    # Escenario 1: Asset normal
    print("\n[Escenario 1] Asset con datos normales")
    generate_batch(
        asset_id="ASSET_001",
        num_files=10,
        size_range_mb=(40, 60),  # Cerca del promedio (50 MB)
        base_path=base_path,
        time_spread_hours=48
    )
    
    # Escenario 2: Asset con algunos archivos grandes
    print("\n[Escenario 2] Asset con datos mixtos")
    generate_batch(
        asset_id="ASSET_002",
        num_files=8,
        size_range_mb=(60, 100),  # Algunos superiores al promedio
        base_path=base_path,
        time_spread_hours=72
    )
    
    # Escenario 3: Asset con un archivo anormalmente grande
    print("\n[Escenario 3] Archivo anormal")
    generate_synthetic_file(
        asset_id="ASSET_003",
        size_mb=200,  # Muy por encima del promedio (30 MB)
        base_path=base_path
    )
    
    print("\n" + "="*60)
    print("✅ GENERACIÓN COMPLETADA")
    print("="*60)
```

---

## Prototipo Funcional

### Script Principal de Prueba

**Archivo: `prototipo\test_fifo_complete.py`**

```python
"""
Script de prueba completo del sistema FIFO.
Demuestra el flujo completo: generar datos → analizar → decidir → borrar.
"""

import os
import sys
from datetime import datetime

# Importar módulos del prototipo
from fifo_engine import FIFOEngine
from synthetic_data_generator import generate_synthetic_file, generate_batch

def print_header(title):
    """Imprime un encabezado formateado."""
    print("\n" + "="*70)
    print(f"  {title}")
    print("="*70)

def print_section(title):
    """Imprime un título de sección."""
    print(f"\n{'─'*70}")
    print(f"  {title}")
    print(f"{'─'*70}")

def show_storage_status(engine, storage_path):
    """Muestra el estado actual del storage."""
    import shutil
    
    usage = shutil.disk_usage(storage_path)
    total_gb = usage.total / (1024**3)
    used_gb = usage.used / (1024**3)
    free_gb = usage.free / (1024**3)
    pct = (used_gb / total_gb) * 100
    
    print(f"\n💾 Estado del Storage:")
    print(f"   Ruta: {storage_path}")
    print(f"   Total: {total_gb:.2f} GB")
    print(f"   Usado: {used_gb:.2f} GB ({pct:.1f}%)")
    print(f"   Libre: {free_gb:.2f} GB")

def show_database_stats(engine):
    """Muestra estadísticas de la base de datos."""
    print(f"\n📊 Estadísticas de Base de Datos:")
    print(f"   Archivos activos: {engine.get_active_files_count()}")
    print(f"   Tamaño total: {engine.get_total_size():.2f} MB")
    
    # Por asset
    cursor = engine.conn.cursor()
    cursor.execute("""
        SELECT asset_id, COUNT(*) as count, SUM(file_size_mb) as size
        FROM data_files
        WHERE status = 'ACTIVE'
        GROUP BY asset_id
    """)
    
    print(f"\n   Por Asset:")
    for row in cursor.fetchall():
        print(f"      {row[0]}: {row[1]} archivos, {row[2]:.2f} MB")

def main():
    print_header("🚀 PRUEBA COMPLETA DEL SISTEMA FIFO")
    
    # Configuración
    db_path = os.path.join("prototipo", "database", "fifo_prototype.db")
    storage_path = os.path.abspath("prototipo\\monitored_storage")
    
    # Verificar que la base de datos existe
    if not os.path.exists(db_path):
        print(f"\n❌ Error: Base de datos no encontrada en {db_path}")
        print(f"   Ejecuta primero: python prototipo\\db_init.py")
        return
    
    # Inicializar motor
    print("\n🔧 Inicializando motor FIFO...")
    engine = FIFOEngine(db_path)
    
    # Mostrar estado inicial
    print_section("📊 ESTADO INICIAL")
    show_storage_status(engine, storage_path)
    show_database_stats(engine)
    
    # =====================================================
    # ESCENARIO 1: Datos Normales (No debe borrar)
    # =====================================================
    print_section("🧪 ESCENARIO 1: Ingreso de Datos Normales")
    print("Generando archivo de tamaño normal para ASSET_001...")
    
    file1 = generate_synthetic_file(
        asset_id="ASSET_001",
        size_mb=45,  # Promedio: 50 MB, esto está bien
        base_path=storage_path
    )
    
    # Analizar
    metadata1 = engine.analyze_incoming_data(file1, "ASSET_001")
    
    # Evaluar
    decision1 = engine.evaluate_deletion_criteria(metadata1)
    
    # Registrar
    engine.register_file(metadata1)
    
    if decision1['cleanup_required']:
        print("\n⚠️  Se activó limpieza (inesperado)")
    else:
        print("\n✅ Resultado esperado: No se requiere limpieza")
    
    # =====================================================
    # ESCENARIO 2: Archivo Anormalmente Grande
    # =====================================================
    print_section("🧪 ESCENARIO 2: Archivo Anormalmente Grande")
    print("Generando archivo 3x más grande que el promedio...")
    
    file2 = generate_synthetic_file(
        asset_id="ASSET_001",
        size_mb=150,  # 3x el promedio (50 MB)
        base_path=storage_path
    )
    
    # Analizar
    metadata2 = engine.analyze_incoming_data(file2, "ASSET_001")
    
    # Evaluar
    decision2 = engine.evaluate_deletion_criteria(metadata2)
    
    if decision2['cleanup_required']:
        print(f"\n✅ Resultado esperado: Limpieza activada")
        print(f"   Razones: {', '.join(decision2['reasons'])}")
        print(f"   Prioridad: {decision2['priority']}")
        
        # Ejecutar limpieza
        result = engine.execute_fifo_cleanup(
            asset_id="ASSET_001",
            space_needed_mb=decision2['space_needed_mb'],
            reason=', '.join(decision2['reasons'])
        )
        
        if result['success']:
            print(f"\n✅ Limpieza exitosa:")
            print(f"   Archivos borrados: {result['files_deleted']}")
            print(f"   Espacio liberado: {result['space_freed_mb']:.2f} MB")
    
    # Registrar nuevo archivo
    engine.register_file(metadata2)
    
    # =====================================================
    # ESCENARIO 3: Múltiples Archivos Pequeños
    # =====================================================
    print_section("🧪 ESCENARIO 3: Lote de Archivos Pequeños")
    print("Generando lote de 15 archivos pequeños...")
    
    files_batch = generate_batch(
        asset_id="ASSET_002",
        num_files=15,
        size_range_mb=(70, 80),
        base_path=storage_path,
        time_spread_hours=72
    )
    
    print(f"\nProcesando {len(files_batch)} archivos...")
    for file_path in files_batch:
        metadata = engine.analyze_incoming_data(file_path, "ASSET_002")
        decision = engine.evaluate_deletion_criteria(metadata)
        
        if decision['cleanup_required']:
            engine.execute_fifo_cleanup(
                asset_id="ASSET_002",
                space_needed_mb=decision['space_needed_mb'],
                reason=', '.join(decision['reasons'])
            )
        
        engine.register_file(metadata)
    
    # =====================================================
    # ESTADO FINAL
    # =====================================================
    print_section("📊 ESTADO FINAL")
    show_storage_status(engine, storage_path)
    show_database_stats(engine)
    
    # Mostrar historial de borrados
    print(f"\n🗑️  Historial de Borrados (últimos 7 días):")
    stats = engine.get_deletion_stats(7)
    
    if stats:
        for row in stats:
            print(f"   {row['asset_id']}: {row['deletions']} archivos, {row['total_freed_mb']:.2f} MB liberados")
    else:
        print(f"   (sin borrados recientes)")
    
    # Cerrar conexión
    engine.close()
    
    print_header("✅ PRUEBA COMPLETADA")
    print("\n💡 Siguientes pasos:")
    print("   1. Revisar logs en consola")
    print("   2. Explorar base de datos: prototipo/database/fifo_prototype.db")
    print("   3. Verificar archivos en: prototipo/monitored_storage/")
    print("   4. Ejecutar consultas SQL para análisis")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\n⚠️  Prueba interrumpida por el usuario")
    except Exception as e:
        print(f"\n❌ Error durante la prueba: {e}")
        import traceback
        traceback.print_exc()
```

---

## Ejecución del Prototipo

### Paso a Paso para Ejecutar

```powershell
# 1. Asegurarse de estar en el directorio correcto
cd C:\Users\IDC INGENIERIA\fifo\FIFOImplementation

# 2. Activar entorno virtual (si lo creaste)
.\venv\Scripts\Activate.ps1

# 3. Inicializar base de datos (si aún no lo hiciste)
python prototipo\db_init.py

# 4. Ejecutar prueba completa
python prototipo\test_fifo_complete.py
```

### Salida Esperada

```
==================================================
  🚀 PRUEBA COMPLETA DEL SISTEMA FIFO
==================================================

🔧 Inicializando motor FIFO...
✅ Conectado a base de datos: prototipo\database\fifo_prototype.db

──────────────────────────────────────────────────
  📊 ESTADO INICIAL
──────────────────────────────────────────────────

💾 Estado del Storage:
   Ruta: C:\...\prototipo\monitored_storage
   Total: 475.00 GB
   Usado: 125.30 GB (26.4%)
   Libre: 349.70 GB

📊 Estadísticas de Base de Datos:
   Archivos activos: 0
   Tamaño total: 0.00 MB

──────────────────────────────────────────────────
  🧪 ESCENARIO 1: Ingreso de Datos Normales
──────────────────────────────────────────────────
...
✅ Resultado esperado: No se requiere limpieza

──────────────────────────────────────────────────
  🧪 ESCENARIO 2: Archivo Anormalmente Grande
──────────────────────────────────────────────────
...
✅ Limpieza exitosa:
   Archivos borrados: 1
   Espacio liberado: 45.00 MB
...
```

---

## Consultas SQL Útiles

### Explorar la Base de Datos

```sql
-- Ver todos los assets configurados
SELECT * FROM assets;

-- Ver archivos activos por asset
SELECT 
    asset_id, 
    COUNT(*) as total_files,
    SUM(file_size_mb) as total_size_mb
FROM data_files
WHERE status = 'ACTIVE'
GROUP BY asset_id;

-- Ver cola FIFO de un asset específico (más viejos primero)
SELECT 
    file_id,
    file_path,
    file_size_mb,
    datetime(created_timestamp) as created_at,
    round((julianday('now') - julianday(created_timestamp)) * 24, 1) as age_hours
FROM data_files
WHERE asset_id = 'ASSET_001' AND status = 'ACTIVE'
ORDER BY created_timestamp ASC
LIMIT 10;

-- Ver historial de borrados
SELECT 
    asset_id,
    deletion_reason,
    COUNT(*) as deletions,
    SUM(space_freed_mb) as total_freed_mb,
    datetime(deletion_timestamp) as deleted_at
FROM deletion_log
GROUP BY asset_id, deletion_reason
ORDER BY deletion_timestamp DESC;

-- Ver próximos archivos a ser borrados (simulación)
SELECT 
    df.asset_id,
    df.file_path,
    df.file_size_mb,
    datetime(df.created_timestamp) as created_at,
    round((julianday('now') - julianday(df.created_timestamp)) * 24, 1) as age_hours,
    a.minimum_retention_hours,
    CASE 
        WHEN (julianday('now') - julianday(df.created_timestamp)) * 24 > a.minimum_retention_hours 
        THEN 'ELEGIBLE'
        ELSE 'PROTEGIDO'
    END as status
FROM data_files df
JOIN assets a ON df.asset_id = a.asset_id
WHERE df.status = 'ACTIVE'
ORDER BY df.created_timestamp ASC;
```

---

## Próximos Pasos

### 1. Desarrollo de GUI (PyQt5)

Crea la interfaz gráfica siguiendo el diseño del documento principal:
- Dashboard con métricas en tiempo real
- Configuración de assets
- Generador de datos de prueba integrado
- Visor de logs

### 2. Implementación del Servicio de Windows

Convierte el motor en un servicio que corra automáticamente:
- Monitoreo continuo
- Ejecución sin login
- Auto-inicio con el sistema

### 3. Testing Avanzado

- Pruebas de estrés (miles de archivos)
- Pruebas de concurrencia (GUI + Servicio)
- Validación de integridad de datos

### 4. Deployment a Producción

- Configurar rutas reales de industrial assets
- Ajustar configuraciones de retención
- Establecer monitoreo y alertas

---

## Conclusión

Has aprendido:
✅ Por qué SQLite es superior a JSON para este caso de uso  
✅ Cómo estructurar una base de datos relacional  
✅ Implementación completa de un algoritmo FIFO  
✅ Manejo de transacciones y integridad de datos  
✅ Generación de datos sintéticos para pruebas  

**El prototipo funcional está listo para pruebas y extensión hacia producción.**
