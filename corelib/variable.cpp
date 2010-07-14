/*
  This file is part of the Grantlee template system.

  Copyright (c) 2009,2010 Stephen Kelly <steveire@gmail.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either version
  2 of the Licence, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "variable.h"

#include "typeaccessors_p.h"

#include <QtCore/QStringList>
#include <QtCore/QMetaEnum>
#include <QtCore/QDebug>

#include "context.h"
#include "util.h"

#include "metaenumvariable_p.h"

using namespace Grantlee;

namespace Grantlee
{

class VariablePrivate
{
public:
  VariablePrivate( Variable *variable )
      : q_ptr( variable ) {

  }

  QVariant resolvePart( const QVariant &variant, const QString &s ) const;

  QString m_varString;
  QVariant m_literal;
  QStringList m_lookups;
  bool m_translate;

  Q_DECLARE_PUBLIC( Variable )
  Variable *q_ptr;
};

}

Variable::Variable( const Variable &other )
    : d_ptr( new VariablePrivate( this ) )
{
  d_ptr->m_varString = other.d_ptr->m_varString;
  d_ptr->m_literal = other.d_ptr->m_literal;
  d_ptr->m_lookups = other.d_ptr->m_lookups;
  d_ptr->m_translate = other.d_ptr->m_translate;
}

Variable::Variable()
    : d_ptr( new VariablePrivate( this ) )
{
}

Variable::~Variable()
{
  delete d_ptr;
}

Variable &Variable::operator=( const Variable & other )
{
  d_ptr->m_varString = other.d_ptr->m_varString;
  d_ptr->m_literal = other.d_ptr->m_literal;
  d_ptr->m_lookups = other.d_ptr->m_lookups;
  d_ptr->m_translate = other.d_ptr->m_translate;
  return *this;
}

Variable::Variable( const QString &var )
    : d_ptr( new VariablePrivate( this ) )
{
  Q_D( Variable );
  d->m_varString = var;

  QVariant v( var );
  if ( v.convert( QVariant::Double ) ) {
    d->m_literal = v;
    if ( !var.contains( '.' ) && !var.contains( 'e' ) ) {
      if ( var.endsWith( '.' ) ) {
//         throw Grantlee::Exception( VariableSyntaxError, QString( "Variable may not end with a dot: %1" ).arg( v.toString() ) );
      }

      d->m_literal = v.toInt();
    }
  } else {
    QString localVar = var;
    if ( var.startsWith( QLatin1String( "_(" ) ) && var.endsWith( QLatin1Char( ')' ) ) ) {
      d->m_translate = true;
      localVar = var.mid( 2, var.size() - 3 );
    }
    if (( localVar.startsWith( '"' ) && localVar.endsWith( '"' ) )
        || ( localVar.startsWith( '\'' ) && localVar.endsWith( '\'' ) ) ) {
      const QString unesc = unescapeStringLiteral( localVar );
      const Grantlee::SafeString ss = markSafe( unesc );
      d->m_literal = QVariant::fromValue<Grantlee::SafeString>( ss );
    } else {
      d->m_lookups = localVar.split( '.' );
    }
  }
}

bool Variable::isValid() const
{
  Q_D( const Variable );
  return !d->m_varString.isEmpty();
}

bool Variable::isConstant() const
{
  Q_D( const Variable );
  return !d->m_literal.isNull();
}

bool Variable::isTrue( Context *c ) const
{
  return variantIsTrue( resolve( c ) );
}

class StaticQtMetaObject : public QObject
{
public:
  static const QMetaObject* _smo() { return &QObject::staticQtMetaObject; }
};

QVariant Variable::resolve( Context *c ) const
{
  Q_D( const Variable );
  QVariant var;
  if ( !d->m_lookups.isEmpty() ) {
    int i = 0;
    if ( d->m_lookups.at( i ) == "Qt" )
    {
      ++i;
      const QString nextPart = d->m_lookups.at( i );
      ++i;

      static const QMetaObject *globalMetaObject = StaticQtMetaObject::_smo();

      bool breakout = false;
      for ( int j = 0; j < globalMetaObject->enumeratorCount(); ++j ) {
        const QMetaEnum me = globalMetaObject->enumerator( j );

        if (me.name() == nextPart)
        {
          const MetaEnumVariable mev(me);
          var = QVariant::fromValue(mev);
          break;
        }

        for ( int k = 0; k < me.keyCount(); ++k )
        {
          if (me.key(k) == nextPart)
          {
            const MetaEnumVariable mev(me, k);
            var = QVariant::fromValue(mev);
            breakout = true;
            break;
          }
        }
        if (breakout)
          break;
      }
      if (!var.isValid())
        return QVariant();

    } else {
      var = c->lookup( d->m_lookups.at( i++ ) );
    }
    while ( i < d->m_lookups.size() ) {
      var = d->resolvePart( var, d->m_lookups.at( i++ ) );
      if ( !var.isValid() )
        return var;
    }
  } else {
    if ( isSafeString( var ) )
      var = QVariant::fromValue( getSafeString( d->m_literal ) );
    else
      var = d->m_literal;
  }

  if ( d->m_translate ) {
//     return gettext(var.toString());
  }


  if ( supportedOutputType( var ) ) {
    return var;
  }
  if ( var.canConvert( QVariant::String ) ) {
    if ( var.convert( QVariant::String ) )
      return QVariant::fromValue<Grantlee::SafeString>( var.toString() );
    return QVariant();
  }
  // Could be a list, hash or enum.
  return var;
}

QVariant VariablePrivate::resolvePart( const QVariant &var, const QString &nextPart ) const
{
  QVariant returnVar;

//   Should be
// * QVariantHash key lookup
// * Property? (member in django)
// * list index
  if ( QVariant::Hash == var.type() ) {
    const QVariantHash hash = var.toHash();
    if ( hash.contains( nextPart ) )
      return hash.value( nextPart );
    return TypeAccessor<QVariantHash>::lookUp( hash, nextPart );
  } else if ( qMetaTypeId< Grantlee::SafeString >() == var.userType() ) {
    return TypeAccessor<SafeString>::lookUp( getSafeString( var ), nextPart );
  } else if ( QMetaType::QObjectStar == var.userType() ) {
    // Can't be const because of invokeMethod.
    const QObject *obj = var.value<QObject *>();
    const QMetaObject *metaObj = obj->metaObject();

    QMetaProperty mp;
    for ( int i = 0; i < metaObj->propertyCount(); ++i ) {
      // TODO only read-only properties should be allowed here.
      // This might also handle the variant messing I hit before.
      mp = metaObj->property( i );

      if ( QString( mp.name() ) != nextPart )
        continue;

      if (mp.isEnumType())
      {
        MetaEnumVariable mev(mp.enumerator(), mp.read( obj ).toInt());
        return QVariant::fromValue(mev);
      }

      return mp.read( obj );
    }
    QMetaEnum me;
    for ( int i = 0; i < metaObj->enumeratorCount(); ++i ) {
      me = metaObj->enumerator( i );

      if (me.name() == nextPart)
      {
        MetaEnumVariable mev(me);
        return QVariant::fromValue(mev);
      }

      const int value = me.keyToValue(nextPart.toLatin1());

      if (value < 0)
        continue;

      const MetaEnumVariable mev(me, value);

      return QVariant::fromValue(mev);
    }
    return QVariant();
  } else if ( qMetaTypeId<MetaEnumVariable>() == var.userType()){
    MetaEnumVariable mev = var.value<MetaEnumVariable>();

    if ( nextPart == "name" )
      return mev.enumerator.name();
    if ( nextPart == "value" )
      return mev.value;
    if ( nextPart == "key" )
      return mev.enumerator.valueToKey( mev.value );
    if ( nextPart == "scope" )
      return mev.enumerator.scope();
    if ( nextPart == "keyCount" )
      return mev.enumerator.keyCount();

    bool ok = false;
    const int listIndex = nextPart.toInt( &ok );
    if (ok)
    {
      if (listIndex >= mev.enumerator.keyCount())
        return QVariant();

      mev.value = mev.enumerator.value(listIndex);
      return QVariant::fromValue(mev);
    }

    return QVariant();
  } else {
    // List index test

    bool ok = false;
    const int listIndex = nextPart.toInt( &ok );
    if ( !ok )
      return QVariant();
    const QVariantList varList = variantToList( var );

    if ( listIndex >= varList.size() )
      return QVariant();
    return varList.at( listIndex );
  }

  return QVariant();
}
