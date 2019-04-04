//----------------------------------------------------------------------------
//   The confidential and proprietary information contained in this file may
//   only be used by a person authorised under and to the extent permitted
//   by a subsisting licensing agreement from ARM Limited or its affiliates.
//
//          (C) COPYRIGHT 2018 ARM Limited or its affiliates
//              ALL RIGHTS RESERVED
//
//   This entire notice must be reproduced on all copies of this file
//   and copies of this file may only be made by a person if such person is
//   permitted to do so under the terms of a subsisting license agreement
//   from ARM Limited or its affiliates.
//----------------------------------------------------------------------------

#ifndef __ATL_BASE_TEMPLATES__
#define __ATL_BASE_TEMPLATES__

#include "ATLTypes.h"
#include "ATLError.h"

#include <exception>
#include <set>

namespace atl {
    class CATLSyncManager {
    protected:

        ~CATLSyncManager() {
        }

    public:
        inline UInt32 Enter() {
            return( atl::EATLErrorOk ) ;
        }

        inline UInt32 Leave() {
            return( atl::EATLErrorOk ) ;
        }
    };

    template<class OWNER>
    class TAutoLock {
    protected:
        OWNER* m_Owner ;

    public:
        inline TAutoLock( OWNER* owner ) : m_Owner( owner ) {
            m_Owner->Enter() ;
        }

        inline ~TAutoLock() {
            if( m_Owner != NULL ) {
                m_Owner->Leave() ;
            }
        }

        inline void Attach( OWNER* owner ) {
            owner->Enter() ;
            m_Owner = owner ;
        }

        inline void DeAttach() {
            m_Owner->Leave() ;
            m_Owner = NULL ;
        }
    } ;

    template <class TT>
    class TSmartPtr {
    protected:
        TT* m_Pointer ;
    public:
        inline TSmartPtr() {
            m_Pointer = NULL ;
        }
        inline TSmartPtr( int ) {
            m_Pointer = NULL ;
        }
        inline TSmartPtr( TT* v ) {
            if( ( m_Pointer = v ) != NULL ) {
                m_Pointer->AddRef() ;
            }
        }
        inline TSmartPtr( const TT* v ) {
            if( ( m_Pointer = const_cast<TT*>( v ) ) != NULL ) {
                m_Pointer->AddRef() ;
            }
        }
        inline TSmartPtr( const TSmartPtr<TT>& src ) {
            if( ( m_Pointer = src.m_Pointer ) != NULL ) {
                m_Pointer->AddRef() ;
            }
        }
        inline ~TSmartPtr() {
            if( m_Pointer != NULL ) {
                m_Pointer->Release() ;
            }
        }

        inline TT* operator->() const {
            return( m_Pointer ) ;
        }

        inline operator TT*() const {
            return( m_Pointer ) ;
        }

        inline TT& operator*() const {
            return( *m_Pointer ) ;
        }

        TT* operator=( TT* src ) {
            if( m_Pointer != NULL ) {
                m_Pointer->Release() ;
            }
            if( ( m_Pointer = src ) != NULL ) {
                m_Pointer->AddRef() ;
            }
            return( m_Pointer ) ;
        }

        TT* operator=( const TSmartPtr<TT>& src ) {
            if( m_Pointer != NULL ) {
                m_Pointer->Release() ;
            }
            if( ( m_Pointer = src.m_Pointer ) != NULL ) {
                m_Pointer->AddRef() ;
            }
            return( m_Pointer ) ;
        }

        inline basebool operator!() const {
            return( m_Pointer == NULL ) ;
        }

        inline basebool operator<( TT* pT ) const {
            return( m_Pointer < pT ) ;
        }

        inline basebool operator==( TT* pT ) const {
            return( m_Pointer == pT ) ;
        }

        inline basebool operator!=( TT* pT ) const {
            return( m_Pointer != pT ) ;
        }
    } ;


    class IATLObserver {
    public:
        virtual void* GetThis() = 0 ;
        virtual UInt32 GetObserver() = 0 ;
        virtual ~IATLObserver() {}
        virtual void Invoke( void* sender ) = 0 ;
    } ;

    template< class TT >
    class TATLObserver : public IATLObserver {
    protected:
        typedef void (TT::* ObserverType)( void* sender ) ;
        ObserverType m_Observer ;
        TT* m_PThis ;
    public:
        TATLObserver( const TATLObserver<TT>& src ) {
            m_PThis = src.m_PThis ;
            m_Observer = src.m_Observer ;
        }

        TATLObserver( TT* pthis, ObserverType pdelegate ) {
            m_PThis = pthis ;
            m_Observer = pdelegate ;
        }

        virtual ~TATLObserver() {}

        virtual void Invoke( void* sender ) {
            (m_PThis->*m_Observer)( sender ) ;
        }

        virtual void* GetThis() {
            return( (void*)m_PThis ) ;
        }

        virtual UInt32 GetObserver() {
            UInt32* ptr = (UInt32*)&m_Observer;
            return( *(ptr) ) ;
        }
    } ;

    template<class SSS = CATLSyncManager>
    class CEventListener : public SSS {
    protected:
        std::set<IATLObserver*> m_Listeners ;

    public:
        inline CEventListener() {
        }

        inline ~CEventListener() {
            SSS::Enter();
            m_Listeners.clear() ;
            SSS::Leave();
        }

        inline CEventListener& operator+=(IATLObserver* listener) {
            TAutoLock< CEventListener<SSS> > lock(this);
            m_Listeners.insert( listener ) ;
            return( *this ) ;
        }

        inline CEventListener& operator-=(IATLObserver* listener) {
            void* pthis = listener->GetThis() ;
            UInt32 pdelegate = listener->GetObserver() ;

            TAutoLock< CEventListener<SSS> > lock(this);
             std::set<IATLObserver*>::iterator it = m_Listeners.begin() ;
            for( it = m_Listeners.begin() ; it != m_Listeners.end() ; ++it ) {
                IATLObserver* d = *it ;
                if( d->GetThis() == pthis && d->GetObserver() == pdelegate ) {
                    m_Listeners.erase( it ) ;
                    break ;
                }
            }
            return( *this ) ;
        }

        inline void Invoke( void* sender ) {
            TAutoLock< CEventListener<SSS> > lock(this);
            std::set<IATLObserver*> calls( m_Listeners ) ;
            lock.DeAttach() ;
            std::set<IATLObserver*>::iterator it = calls.begin() ;
            for( it = calls.begin() ; it != calls.end() ; ++it ) {
                IATLObserver *ptr = *it ;
                ptr->Invoke( sender ) ;
            }
        }
    } ;
} // atl namespace

#endif // __ATL_BASE_TEMPLATES__
